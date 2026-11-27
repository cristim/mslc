#include "sema/emitter.h"

namespace mslc {

// A struct value that is computed rather than named: S{...}, a call, or a conditional with
// such an arm. A conditional of named arms may yield a place, so it is not one.
bool Emitter::isComputedValue(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::Identifier: return false;
		case ExpressionKind::Member: return isComputedValue(*expression.left);
		case ExpressionKind::Index: return indexOperatorOwner(expression) != nullptr || isComputedValue(*expression.left);
		case ExpressionKind::Comma: return isComputedValue(*expression.right);
		// One prvalue arm makes the whole conditional a prvalue.
		case ExpressionKind::Conditional:
			return isComputedValue(*expression.arguments[0]) || isComputedValue(*expression.arguments[1]);
		case ExpressionKind::Call:
		case ExpressionKind::InitList: return true;
		default: return false;
	}
}

// A member or element of a computed value: a prvalue, so nothing can be stored to it
// or bound to a reference.
bool Emitter::isTemporaryPart(const Expression& target) {
	return (target.kind == ExpressionKind::Member || target.kind == ExpressionKind::Index)
		&& isComputedValue(target);
}

// The messages for a place that is a member of a conditional or comma whose arms are
// named: Apple takes them, mslc does not lower them.
void rejectPlaceThroughConditional(const Expression& target, const char* what) {
	if (target.kind != ExpressionKind::Member && target.kind != ExpressionKind::Index) {
		return;
	}
	const Expression* root = &target;
	while (root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) {
		root = root->left.get();
	}
	if (root->kind == ExpressionKind::Conditional) {
		throw CompileError(std::string(what) + " a member or element of a conditional expression is not "
			"supported; use the variable in an if instead");
	}
	if (root->kind == ExpressionKind::Comma) {
		throw CompileError(std::string(what) + " a member or element of a comma expression is not "
			"lowered yet; use its right operand on its own");
	}
}

// The variable a non-const reference parameter names: the caller's own, so a
// store in the helper is a store to it. Only a place in the thread's own
// storage can be passed, since the parameter is a pointer into it.
Id Emitter::referenceArgument(const Expression& argument, const Parameter& parameter,
	const std::string& function, size_t position, std::vector<std::pair<Id, Id>>& copyOuts) {
	const std::string what = "argument " + std::to_string(position) + " of the call to \"" + function + "\"";
	if (argument.kind != ExpressionKind::Identifier && argument.kind != ExpressionKind::Index
		&& argument.kind != ExpressionKind::Member) {
		throw CompileError(what + " has to be a variable, an element or a member: parameter \""
			+ parameter.name + "\" is a reference to it");
	}
	if (bindsTemporary(argument)) {
		throw CompileError("no matching function for call to \"" + function + "\": " + what
			+ " is a temporary, which parameter \"" + parameter.name + "\" cannot bind as a reference");
	}
	if (isVectorComponent(argument)) {
		throw CompileError(what + " is a component of a vector, which a reference parameter cannot "
			"name (Apple refuses it too)");
	}
	rejectPlaceThroughConditional(argument, "passing by reference");

	const Expression& root = storeRoot(argument);
	if (root.kind == ExpressionKind::Identifier && isComputedValue(argument)) {
		throw CompileError(what + " is a member or element of a temporary, which parameter \""
			+ parameter.name + "\" cannot bind as a reference");
	}
	if (root.kind == ExpressionKind::Identifier) {
		const Binding* binding = findBinding(root);
		if ((binding && binding->readOnly) || (!binding && _constants.count(root.name))) {
			throw CompileError(what + " is const or in constant memory, and parameter \""
				+ parameter.name + "\" is a reference that can change it");
		}
	}

	const Id address = emitPlaceAddress(argument);
	const auto storageClass = _types.storageClassOf(_builder.typeOf(address));
	if (!storageClass || *storageClass != spirv::StorageClass::Function) {
		throw CompileError(what + " is not a variable of this function; a reference parameter takes "
			"a local, a member of one, or a reference parameter of the caller");
	}
	if (valueTypeAt(address) != declaredTypeOf(parameter.type)
		|| (parameter.type.enumName == "metal::component") != isComponentValue(argument)) {
		throw CompileError(what + " is not the type of parameter \"" + parameter.name + "\", "
			+ typeName(parameter.type) + "; a conversion would make a temporary");
	}

	// A function parameter has to be a pointer to a variable, which an element
	// or a member of one is not, so those go through a temporary copied back
	// after the call.
	if (const Binding* named = findBinding(argument); named && !named->isAddress) {
		return address;
	}

	const Id type = declaredTypeOf(parameter.type);
	const Id temporary = _builder.emitDeclTyped(spirv::OpVariable,
		_types.pointer(spirv::StorageClass::Function, type),
		{ static_cast<uint32_t>(spirv::StorageClass::Function) });
	_builder.emit(spirv::OpStore, { temporary, loadFrom(address, type) });
	copyOuts.emplace_back(address, temporary);
	return temporary;
}

// Whether a name, an element or a member lives in the constant address space: a constant buffer, a
// file-scope constant, or a constant reference parameter of the function being emitted.
bool Emitter::inConstantSpace(const Expression& place) {
	if (isComputedValue(place)) { return false; }
	const Expression& root = storeRoot(place);
	if (const Type* pointer = pointerMemberType(root)) {
		return pointer->addressSpace == AddressSpace::Constant;
	}
	if (root.kind != ExpressionKind::Identifier) { return false; }
	const auto& table = root.fileScope ? _fileScopeBound : _bindings;
	if (const auto binding = table.find(root.name); binding != table.end()) {
		return binding->second.pointeeMsl.addressSpace == AddressSpace::Constant;
	}
	return _constants.count(root.name) > 0;
}

// The type a name, an element or a member has, as the source spelled it; a buffer pointer counts as an
// array. Nothing for an expression that is not one of these.
std::optional<Type> Emitter::placeType(const Expression& place) {
	switch (place.kind) {
		case ExpressionKind::Identifier: {
			// "::x" is the file-scope x, whatever local hides it.
			const auto binding = place.fileScope ? _bindings.end() : _bindings.find(place.name);
			if (binding != _bindings.end()) {
				Type type = binding->second.pointeeMsl;
				if (binding->second.isBuffer) {
					if (type.arrayLength) {
						type.innerArrayLengths.insert(type.innerArrayLengths.begin(), *type.arrayLength);
					}
					type.arrayLength = 1;
				}
				return type;
			}
			for (const VariableDeclaration& global: _unit.globals) {
				if (global.name == place.name) {
					return global.type;
				}
			}
			return std::nullopt;
		}
		case ExpressionKind::Member: {
			// A member of "::s" finds its struct from the file-scope s, not the local that hides it.
			const Expression* root = place.left.get();
			while (root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) {
				root = root->left.get();
			}
			const StructDecl* decl = nullptr;
			if (root->kind == ExpressionKind::Identifier && root->fileScope) {
				const std::optional<Type> owner = placeType(*place.left);
				decl = owner && !owner->isPointer ? _unit.findStruct(owner->namedType) : nullptr;
			} else {
				decl = structOf(*place.left);
			}
			if (!decl) {
				return std::nullopt;
			}
			std::string name;
			const size_t field = fieldIndexOf(*decl, place.memberName, name);
			return field == decl->fields.size() ? std::nullopt : std::optional<Type>(decl->fields[field].type);
		}
		case ExpressionKind::Index: {
			if (const StructDecl* owner = indexOperatorOwner(place)) {
				const HelperFunction* op = indexOperatorHelper(*owner);
				return !op || place.arguments.size() != 1 || returnsVoid(*op->definition)
					? std::nullopt : std::optional<Type>(op->definition->returnType);
			}
			std::optional<Type> type = placeType(*place.left);
			if (!type) {
				return std::nullopt;
			}
			// An index of a pointer member, s.p[i] or *s.p, is its pointee.
			if (type->isPointer) {
				type->isPointer = false;
				return type;
			}
			if (type->arrayLength) {
				type->dropOuterDimension();
			} else if (type->isMatrix()) {
				type->matrixColumns = 0;
			} else if (type->vectorWidth > 1) {
				type->vectorWidth = 0;
			} else {
				return std::nullopt;
			}
			return type;
		}
		default: return std::nullopt;
	}
}

// The type a sizeof operand that is a name, an element or a member has, as the source spelled it:
// a packed vector is 12 bytes where the value of one is a float3 of 16. A swizzle of a vector
// place is an unpacked vector of its length.
std::optional<Type> Emitter::sizeofPlaceType(const Expression& place) {
	if (place.kind == ExpressionKind::Member && !structOf(*place.left)) {
		std::optional<Type> base = isComputedValue(*place.left)
			? sizeofExpressionType(*place.left) : sizeofPlaceType(*place.left);
		if (base && !base->namedType.empty() && !base->isPointer) {
			// structOf found the struct of a local that hides "::name"; placeType reads the file-scope one.
			return placeType(place);
		}
		const size_t lanes = place.memberName.size();
		if (!base || !base->isVector() || base->isMatrix() || base->arrayLength || base->isPointer
			|| lanes < 1 || lanes > 4) {
			return std::nullopt;
		}
		// Letters of one set, each naming a component the vector has.
		const std::string letters = place.memberName.find_first_of("xyzw") == 0 ? "xyzw" : "rgba";
		for (const char letter: place.memberName) {
			const size_t index = letters.find(letter);
			if (index == std::string::npos || index >= base->vectorWidth) {
				return std::nullopt;
			}
		}
		base->isPacked = false;
		base->vectorWidth = lanes > 1 ? static_cast<uint32_t>(lanes) : 0;
		return base;
	}
	if ((place.kind == ExpressionKind::Member || place.kind == ExpressionKind::Index)
		&& isComputedValue(*place.left) && !sizeofExpressionType(*place.left)) return std::nullopt;
	return placeType(place);
}

// The type of a sizeof operand worked out from the source alone, by C's rules, without building a
// value: nothing in the operand runs or is emitted. A packed vector keeps its packed type through
// the unary and binary operators (Apple: sizeof is 12). Nothing for an operand whose type is not
// certain, so the caller refuses it rather than guess.
std::optional<Type> Emitter::sizeofExpressionType(const Expression& expression) {
	const auto plain = [](const Type& type) {
		return !type.isPointer && !type.arrayLength && !type.isMatrix() && type.namedType.empty()
			&& type.enumName.empty() && type.resource == ResourceKind::None && !type.isAtomic
			&& !type.isAuto && type.scalar != ScalarKind::Void && type.scalar != ScalarKind::Double;
	};
	const auto scalarOf = [](ScalarKind kind) { Type type; type.scalar = kind; return type; };
	// The integer promotions: anything narrower than an int, a bool included, is an int.
	const auto promoted = [&](Type type) {
		if (type.vectorWidth == 0 && !isFloatKind(type.scalar)
			&& (type.scalar == ScalarKind::Bool || promotedKind(type.scalar) != type.scalar)) {
			type.scalar = ScalarKind::Int;
		}
		return type;
	};
	// The usual arithmetic conversions of two scalar kinds: a float beats an integer and a float is
	// the wider of the two, then C's integer rules.
	const auto commonKind = [](ScalarKind left, ScalarKind right) {
		const bool leftFloat = isFloatKind(left);
		const bool rightFloat = isFloatKind(right);
		if (leftFloat || rightFloat) {
			return leftFloat && rightFloat ? (left == ScalarKind::Float || right == ScalarKind::Float
				? ScalarKind::Float : ScalarKind::Half) : leftFloat ? left : right;
		}
		const auto promoteInt = [](ScalarKind kind) {
			return kind == ScalarKind::Bool ? ScalarKind::Int : promotedKind(kind);
		};
		return commonIntegerKind(promoteInt(left), promoteInt(right));
	};
	// What a binary operator of two plain arithmetic operands gives: scalars meet in the usual
	// conversions; two vectors of one element type and width give the left one's type, packed or
	// not (Apple: packed - float3 is 12, float3 - packed is 16); a vector beside a scalar of its own
	// element type is the vector; anything else is not modelled.
	const auto arithmetic = [&](const Type& left, const Type& right) -> std::optional<Type> {
		if (left.isScalar() && right.isScalar()) {
			return scalarOf(commonKind(left.scalar, right.scalar));
		}
		if (left.scalar != right.scalar || left.scalar == ScalarKind::Bool || (left.vectorWidth != right.vectorWidth
			&& !left.isScalar() && !right.isScalar())) {
			return std::nullopt;
		}
		return left.isScalar() ? right : left;
	};

	// What a binary operator gives for operands of these types, or nothing where the operator does not
	// take them (a shift or a remainder of a float) or the result is not modelled.
	const auto binaryType = [&](BinaryOperator op, const Type& left, const Type& right) -> std::optional<Type> {
		// A pointer moved by an integer is that pointer.
		const bool offset = op == BinaryOperator::Add || op == BinaryOperator::Subtract;
		const auto integer = [&](const Type& type) { return plain(type) && type.isScalar() && !isFloatKind(type.scalar); };
		if (offset && left.isPointer && !left.arrayLength && integer(right)) {
			return left;
		}
		if (op == BinaryOperator::Add && right.isPointer && !right.arrayLength && integer(left)) {
			return right;
		}
		if (!plain(left) || !plain(right)) {
			return std::nullopt;
		}
		switch (op) {
			case BinaryOperator::Less: case BinaryOperator::LessEqual: case BinaryOperator::Greater:
			case BinaryOperator::GreaterEqual: case BinaryOperator::Equal: case BinaryOperator::NotEqual:
			case BinaryOperator::LogicalAnd: case BinaryOperator::LogicalOr:
				return left.isScalar() && right.isScalar() ? std::optional<Type>(scalarOf(ScalarKind::Bool)) : std::nullopt;
			case BinaryOperator::ShiftLeft: case BinaryOperator::ShiftRight:
				return left.isScalar() && right.isScalar() && !isFloatKind(left.scalar) && !isFloatKind(right.scalar)
					? std::optional<Type>(promoted(left)) : std::nullopt;
			case BinaryOperator::Modulo:
			case BinaryOperator::BitAnd: case BinaryOperator::BitOr: case BinaryOperator::BitXor:
				return isFloatKind(left.scalar) || isFloatKind(right.scalar) ? std::nullopt : arithmetic(left, right);
			default:
				return arithmetic(left, right);
		}
	};
	// Whether a value of type `from` converts to an object of type `to` the way Apple accepts: any
	// scalar to any scalar, otherwise the same vector, matrix, struct or pointer.
	const auto converts = [&](const Type& to, const Type& from) {
		if (to.isPointer || from.isPointer) {
			return to.isPointer && from.isPointer && !to.arrayLength && !from.arrayLength && to.scalar == from.scalar
				&& to.namedType == from.namedType && to.vectorWidth == from.vectorWidth
				&& to.matrixColumns == from.matrixColumns && to.isPacked == from.isPacked;
		}
		if (to.arrayLength || from.arrayLength || to.resource != ResourceKind::None || from.resource != ResourceKind::None
			|| to.isAuto || from.isAuto || !to.enumName.empty() || !from.enumName.empty() || to.isAtomic || from.isAtomic) {
			return false;
		}
		if (!to.namedType.empty() || !from.namedType.empty()) {
			return to.namedType == from.namedType;
		}
		if (to.isScalar() && from.isScalar() && !to.isMatrix() && !from.isMatrix()) {
			return to.scalar != ScalarKind::Void && from.scalar != ScalarKind::Void;
		}
		return to.scalar == from.scalar && to.vectorWidth == from.vectorWidth && to.matrixColumns == from.matrixColumns;
	};
	// Whether a place cannot be written: a const object, one in the constant address space, or a member of one.
	const auto readOnly = [&](const Expression& place) {
		const Expression* found = &storeRoot(place);
		if (pointerMemberType(*found)) {
			// Through the pointer its pointee decides; the pointer itself is part of its struct.
			if (found != &place) {
				return readOnlyRoot(*found);
			}
			found = &storeRoot(*place.left);
		}
		const Expression& root = *found;
		if (root.kind != ExpressionKind::Identifier) {
			return true;
		}
		const auto binding = root.fileScope ? _bindings.end() : _bindings.find(root.name);
		if (binding != _bindings.end()) {
			return binding->second.readOnly || binding->second.pointeeMsl.isConst;
		}
		return true;
	};

	switch (expression.kind) {
		case ExpressionKind::IntLiteral: return scalarOf(expression.intKind);
		case ExpressionKind::FloatLiteral: return scalarOf(expression.floatIsHalf ? ScalarKind::Half : ScalarKind::Float);
		case ExpressionKind::BoolLiteral: return scalarOf(ScalarKind::Bool);
		case ExpressionKind::Sizeof: return scalarOf(ScalarKind::ULong);
		case ExpressionKind::Construct: return expression.constructType;
		case ExpressionKind::InitList:
			if (expression.namesConcreteStruct && expression.constructType) {
				return expression.constructType;
			}
			return std::nullopt;
		case ExpressionKind::Identifier:
		case ExpressionKind::Member:
		case ExpressionKind::Index: {
			if (expression.kind == ExpressionKind::Identifier && !expression.fileScope) {
				const auto binding = _bindings.find(expression.name);
				if (binding != _bindings.end() && binding->second.isBuffer) {
					Type pointer = binding->second.pointeeMsl;
					pointer.isPointer = true;
					pointer.arrayLength.reset();
					pointer.innerArrayLengths.clear();
					return pointer;
				}
				// A helper's pointer parameter is bound like a local array, so a local that hides it
				// cannot be told apart there. An entry point's is a buffer binding, which a hiding
				// local is not.
				if (_helper && namesPointerParameter(expression.name)) {
					return std::nullopt;
				}
			} else if (expression.kind == ExpressionKind::Index) {
				if (const StructDecl* owner = indexOperatorOwner(expression)) {
					const HelperFunction* op = indexOperatorHelper(*owner);
					if (!op || expression.arguments.size() != 1 || returnsVoid(*op->definition)
						|| !sizeofExpressionType(*expression.left) || !validMemberReceiver(*op->definition, *expression.left)) {
						return std::nullopt;
					}
					const Parameter& idx = op->definition->parameters[1];
					const std::optional<Type> arg = sizeofExpressionType(*expression.arguments[0]);
					return !arg || idx.isReference || idx.type.addressSpace != AddressSpace::None || !converts(idx.type, *arg)
						? std::nullopt : std::optional<Type>(op->definition->returnType);
				}
			}
			return sizeofPlaceType(expression);
		}
		case ExpressionKind::Conditional: {
			const std::optional<Type> condition = sizeofExpressionType(*expression.left);
			if (!condition || !plain(*condition) || !condition->isScalar()) {
				return std::nullopt;
			}
			const std::optional<Type> yes = sizeofExpressionType(*expression.arguments[0]);
			const std::optional<Type> no = sizeofExpressionType(*expression.arguments[1]);
			if (yes && no && yes->isPointer && no->isPointer && !yes->arrayLength && !no->arrayLength
				&& yes->scalar == no->scalar && yes->namedType == no->namedType && yes->vectorWidth == no->vectorWidth
				&& yes->matrixColumns == no->matrixColumns && yes->isPacked == no->isPacked
				&& yes->addressSpace == no->addressSpace) {
				return yes;
			}
			if (!yes || !no || !plain(*yes) || !plain(*no)) {
				return std::nullopt;
			}
			if (yes->scalar == no->scalar && yes->vectorWidth == no->vectorWidth) {
				return yes;
			}
			return yes->isScalar() && no->isScalar() ? std::optional<Type>(scalarOf(commonKind(yes->scalar, no->scalar)))
				: std::nullopt;
		}
		case ExpressionKind::Comma:
			// The left side is not built, but Apple still rejects one that is not an expression.
			return sizeofExpressionType(*expression.left) ? sizeofExpressionType(*expression.right) : std::nullopt;
		case ExpressionKind::Call: {
			// The return type of the one function the name can find: any overload, template, library
			// function beside it or argument-dependent lookup would need the arguments' types.
			if (expression.left->kind != ExpressionKind::Identifier || expression.left->hasTemplateArguments
				|| (!expression.left->fileScope && _bindings.count(expression.left->name) > 0)) {
				return std::nullopt;
			}
			std::set<std::string> adl;
			const std::vector<std::string> names = namesOfCall(expression, &adl);
			if (names.size() != 1 || !adl.empty() || !visibleTemplates(names, adl).empty()) {
				return std::nullopt;
			}
			const std::vector<HelperFunction*> set = helpersNamed(names.front());
			if (set.empty()) {
				const std::string& callee = names.front();
				const bool fpSelect = callee == "metal::precise::select" || callee == "metal::fast::select";
				const bool isSelect = callee == "select" || callee == "metal::select" || fpSelect;
				const std::optional<MathBuiltin> builtin = isSelect ? std::nullopt : findScopedMathBuiltin(callee);
				const bool isDerivative = builtin && (builtin->shape == MathShape::Fwidth
					|| builtin->shape == MathShape::Dfdx || builtin->shape == MathShape::Dfdy);
				if ((isSelect ? expression.arguments.size() != 3 : (!builtin || expression.arguments.size() != builtin->arity))
					|| (isDerivative && (!_entryPoint || _entryPoint->stage != Stage::Fragment))) {
					return std::nullopt;
				}
				std::vector<Type> args;
				for (const ExpressionPtr& entry: expression.arguments) {
					const std::optional<Type> arg = sizeofExpressionType(*entry);
					if (!arg || !plain(*arg) || arg->isPacked || (!isSelect && arg->scalar == ScalarKind::Bool)) {
						return std::nullopt;
					}
					args.push_back(*arg);
				}
				if (isSelect) {
					Type out = args[0].isVector() ? args[0] : args[1];
					if ((fpSelect && !isFloatKind(out.scalar))
						|| (args[0].isVector() == args[1].isVector()
							? (args[0].scalar != args[1].scalar || args[0].vectorWidth != args[1].vectorWidth)
							: !converts(scalarOf(out.scalar), args[0].isVector() ? args[1] : args[0]))
						|| (args[2].isVector() && (!out.isVector() || args[2].scalar != ScalarKind::Bool
							|| args[2].vectorWidth != out.vectorWidth))) {
						return std::nullopt;
					}
					out.isConst = false;
					out.addressSpace = AddressSpace::None;
					return out;
				}
				const size_t sharing = builtin->shape == MathShape::Refract ? 2 : args.size();
				const bool takesIntegers = builtin->signedInstruction != kNoInstruction;
				Type type = args[0];
				for (size_t i = 0; i < sharing; ++i) {
					if (args[i].isVector()) { type = args[i]; break; }
					if (!isFloatKind(type.scalar) && isFloatKind(args[i].scalar)) { type = args[i]; }
				}
				const bool takesScalars = builtin->shape != MathShape::VectorToScalar
					&& builtin->shape != MathShape::Vector && builtin->shape != MathShape::Dot
					&& builtin->shape != MathShape::Refract && builtin->shape != MathShape::Cross;
				if ((isDerivative ? !isFloatKind(type.scalar)
						: (!isFloatKind(type.scalar) && (!takesIntegers
							|| (!isSignedInteger(type.scalar) && builtin->unsignedInstruction == kNoInstruction))))
					|| (!takesScalars && !type.isVector())
					|| (builtin->shape == MathShape::Cross && type.vectorWidth != 3)
					|| (builtin->shape == MathShape::Refract && args[2].isVector())) {
					return std::nullopt;
				}
				for (size_t i = 0; i < sharing; ++i) {
					if (args[i].scalar == type.scalar && args[i].vectorWidth == type.vectorWidth) { continue; }
					const bool converts = !type.isVector() && !takesIntegers && !isFloatKind(args[i].scalar);
					if (args[i].isVector() || (!type.isVector() && !converts)
						|| (!isFloatKind(type.scalar) && isFloatKind(args[i].scalar))) {
						return std::nullopt;
					}
				}
				if (builtin->shape == MathShape::VectorToScalar || builtin->shape == MathShape::Dot) {
					return scalarOf(type.scalar);
				}
				type.isConst = false;
				type.addressSpace = AddressSpace::None;
				return type;
			}
			std::set<std::string> beside = expression.left->stdlibBeside;
			if (names.front().compare(0, stdlibPrefix.size(), stdlibPrefix) == 0 && !isMemberName(names.front())) {
				beside.insert(names.front().substr(stdlibPrefix.size()));
			}
			const bool competesWithStdlib = std::any_of(beside.begin(), beside.end(), [](const std::string& sub) {
				return sub == "select" || sub == "precise::select" || sub == "fast::select"
					|| sub == "all" || sub == "any" || findPackBuiltin(sub) != nullptr
					|| findScopedMathBuiltin("metal::" + sub).has_value();
			});
			if (set.size() != 1 || competesWithStdlib
				|| set.front()->definition->parameters.size() != expression.arguments.size()) {
				return std::nullopt;
			}
			// Each argument has to be one the parameter takes: a mismatch is an error in Apple, and
			// a reference parameter binds to a place, which is not modelled.
			for (size_t i = 0; i < expression.arguments.size(); ++i) {
				const Parameter& parameter = set.front()->definition->parameters[i];
				const std::optional<Type> argument = sizeofExpressionType(*expression.arguments[i]);
				if (!argument || parameter.isReference || parameter.type.addressSpace != AddressSpace::None
					|| !converts(parameter.type, *argument)) {
					return std::nullopt;
				}
			}
			const Type& result = set.front()->definition->returnType;
			return result.scalar == ScalarKind::Void && result.namedType.empty() && !result.isPointer
				? std::nullopt : std::optional<Type>(result);
		}
		case ExpressionKind::Assign: {
			// The type of the place written; the value is never stored.
			const ExpressionKind target = expression.left->kind;
			if (target != ExpressionKind::Identifier && target != ExpressionKind::Member && target != ExpressionKind::Index) {
				return std::nullopt;
			}
			const std::optional<Type> place = sizeofExpressionType(*expression.left);
			const std::optional<Type> value = sizeofExpressionType(*expression.right);
			if (!place || !value || place->isConst || isComputedValue(*expression.left) || readOnly(*expression.left)
				|| (!expression.compoundOperator && !converts(*place, *value))) {
				return std::nullopt;
			}
			if (expression.compoundOperator) {
				// x op= y is x = x op y, which has to be a valid operator and convert back.
				if (place->isPointer) {
					return std::nullopt;
				}
				const std::optional<Type> result = binaryType(*expression.compoundOperator, *place, *value);
				if (!result || !converts(*place, *result)) {
					return std::nullopt;
				}
			}
			return place;
		}
		case ExpressionKind::Unary: {
			const std::optional<Type> operand = sizeofExpressionType(*expression.left);
			const bool incdec = expression.unaryOperator == UnaryOperator::PreIncrement
				|| expression.unaryOperator == UnaryOperator::PreDecrement
				|| expression.unaryOperator == UnaryOperator::PostIncrement
				|| expression.unaryOperator == UnaryOperator::PostDecrement;
			if (!operand || expression.unaryOperator == UnaryOperator::AddressOf
				|| !(plain(*operand) || (incdec && operand->isPointer && !operand->arrayLength))) {
				return std::nullopt;
			}
			switch (expression.unaryOperator) {
				case UnaryOperator::Not: return operand->isScalar() ? std::optional<Type>(scalarOf(ScalarKind::Bool)) : std::nullopt;
				case UnaryOperator::PreIncrement:
				case UnaryOperator::PreDecrement:
				case UnaryOperator::PostIncrement:
				case UnaryOperator::PostDecrement: {
					const ExpressionKind target = expression.left->kind;
					return (target == ExpressionKind::Identifier || target == ExpressionKind::Member
						|| target == ExpressionKind::Index) && operand->scalar != ScalarKind::Bool && !operand->isConst
						&& !isComputedValue(*expression.left) && !readOnly(*expression.left) ? operand : std::nullopt;
				}
				case UnaryOperator::BitNot:
					return isFloatKind(operand->scalar) || operand->scalar == ScalarKind::Bool ? std::nullopt
						: std::optional<Type>(promoted(*operand));
				default: return promoted(*operand);
			}
		}
		case ExpressionKind::Binary: {
			const std::optional<Type> left = sizeofExpressionType(*expression.left);
			const std::optional<Type> right = sizeofExpressionType(*expression.right);
			return left && right ? binaryType(expression.binaryOperator, *left, *right) : std::nullopt;
		}
		default:
			return std::nullopt;
	}
}

bool Emitter::namesPointerParameter(const std::string& name) const {
	const FunctionDecl* function = _helper ? _helper : _entryPoint;
	return function && std::any_of(function->parameters.begin(), function->parameters.end(),
		[&](const Parameter& parameter) { return parameter.name == name && parameter.type.isPointer; });
}

// The size sizeof gives the expression's type; the expression itself is never evaluated. Only what
// can be typed from the declarations, or an arithmetic result of such parts, is sized: a guess at
// the type of anything else would be a silent wrong size.
uint64_t Emitter::sizeofOperand(const Expression& operand) {
	const auto sized = [&](const Type& type) {
		if (type.scalar == ScalarKind::Void && type.namedType.empty() && !type.isPointer
			&& type.resource == ResourceKind::None && !type.isAuto) {
			throw CompileError("the type of this sizeof operand is not recorded; write sizeof(<type>)");
		}
		return sizeofType(type, _unit);
	};

	if (const std::optional<Type> type = sizeofExpressionType(operand)) {
		return sized(*type);
	}
	throw CompileError("sizeof of this expression is not lowered: its type is not one mslc reads "
		"from the declarations; write sizeof(<type>)");
}

// A place whose value is a whole vector.
bool Emitter::isVectorPlace(const Expression& place) {
	const std::optional<Type> type = placeType(place);
	return type && !type->arrayLength && type->vectorWidth > 1 && !type->isMatrix();
}

// The type of a member or element chain of a computed value, by the struct declarations alone.
std::optional<Type> Emitter::computedPartType(const Expression& part) {
	if (part.kind == ExpressionKind::Member) {
		const StructDecl* decl = structOf(*part.left);
		std::string name;
		const size_t field = decl ? fieldIndexOf(*decl, part.memberName, name) : 0;
		return decl && field != decl->fields.size() ? std::optional<Type>(decl->fields[field].type)
			: std::nullopt;
	}
	if (part.kind != ExpressionKind::Index) {
		return arrayExpressionType(part);
	}
	if (const StructDecl* owner = indexOperatorOwner(part)) {
		const HelperFunction* op = indexOperatorHelper(*owner);
		return !op || returnsVoid(*op->definition) ? std::nullopt
			: std::optional<Type>(op->definition->returnType);
	}
	std::optional<Type> type = computedPartType(*part.left);
	if (!type) {
		return std::nullopt;
	}
	if (type->arrayLength) {
		type->dropOuterDimension();
	} else if (type->isMatrix()) {
		type->matrixColumns = 0;
	} else {
		return std::nullopt;
	}
	return type;
}

// A member or element of a temporary that a reference cannot bind. A column of a matrix is left
// out: Metal's matrix operator[] gives an lvalue even for a prvalue, so Apple takes that.
bool Emitter::bindsTemporary(const Expression& place) {
	if (!isTemporaryPart(place)) {
		return false;
	}
	for (const Expression* node = &place; node->kind == ExpressionKind::Member
		|| node->kind == ExpressionKind::Index; node = node->left.get()) {
		if (node->kind != ExpressionKind::Index) {
			continue;
		}
		if (indexOperatorOwner(*node)) return true;
		// An element of a base that cannot be typed here (a call or a conditional) might be a matrix
		// column, so only a base known not to be a matrix may say temporary.
		const std::optional<Type> base = computedPartType(*node->left);
		if (!base || (!base->arrayLength && base->isMatrix())) {
			return false;
		}
	}
	return true;
}

// A component of a vector (v.x, v[1], m[1][0]), which no reference binds (Apple refuses it too).
bool Emitter::isVectorComponent(const Expression& place) {
	if (place.kind == ExpressionKind::Member) {
		return !structOf(*place.left);
	}
	return place.kind == ExpressionKind::Index && isVectorPlace(*place.left);
}

// A reference into constant memory is passed as the value it names: the memory cannot change under
// the callee. Its argument has to be a place of that space and of exactly the parameter's type.
Id Emitter::constantReferenceArgument(const Expression& argument, const Parameter& parameter,
	const std::string& function, size_t position, Id prepared) {
	const std::string what = "argument " + std::to_string(position) + " of the call to \"" + function + "\"";
	const std::string reason = ", and parameter \"" + parameter.name + "\" is a reference to constant memory";
	if (argument.kind != ExpressionKind::Identifier && argument.kind != ExpressionKind::Index
		&& argument.kind != ExpressionKind::Member) {
		throw CompileError(what + " has to be a variable, an element or a member of constant memory" + reason);
	}
	if (isVectorComponent(argument)) {
		throw CompileError(what + " is a component of a vector, which a reference parameter cannot name" + reason);
	}
	if (!inConstantSpace(argument)) {
		throw CompileError(what + " is not in the constant address space" + reason
			+ "; Apple does not bind it to a value of another address space");
	}
	const auto tag = enumOf(argument);
	if (!parameter.type.enumName.empty()) {
		requireEnumValue(parameter.type.enumName, argument, what);
	} else {
		requireEnumValue("", argument, what);
	}
	const Id value = prepared != InvalidId ? prepared : emitExpression(argument);
	// A struct read out of a buffer has the buffer's layout, which the by-value conversion maps.
	const bool structValue = !parameter.type.namedType.empty() && !parameter.type.isPointer;
	if ((!structValue && _builder.typeOf(value) != declaredTypeOf(parameter.type))
		|| (tag && *tag != parameter.type.enumName)
		|| (parameter.type.enumName == "metal::component" && (!tag || *tag != "metal::component"))) {
		throw CompileError(what + " is not the type of parameter \"" + parameter.name + "\", "
			+ typeName(parameter.type) + "; a conversion would make a temporary in another address space");
	}
	return value;
}

} // namespace mslc
