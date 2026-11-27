#include "sema/emitter.h"

namespace mslc {

// True when the expression reads something only known when the shader runs: a
// parameter, a local that is not a constant, an element of a buffer, or a
// store. Used to refuse such an initialiser on a constexpr; a form that is
// not recognised as runtime is left to the lowering, which folds what it can.
bool Emitter::readsRuntimeValue(const Expression& expression) const {
	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
		case ExpressionKind::FloatLiteral:
		case ExpressionKind::BoolLiteral:
		case ExpressionKind::Sizeof:
			return false;
		case ExpressionKind::Assign:
			return true;
		case ExpressionKind::Unary:
			if (expression.unaryOperator == UnaryOperator::PreIncrement
				|| expression.unaryOperator == UnaryOperator::PreDecrement
				|| expression.unaryOperator == UnaryOperator::PostIncrement
				|| expression.unaryOperator == UnaryOperator::PostDecrement) {
				return true;
			}
			break;
		case ExpressionKind::Identifier: {
			const auto local = expression.fileScope ? _bindings.end() : _bindings.find(expression.name);
			if (local == _bindings.end()) {
				return false;
			}
			return _localFolded.count(local->second.id) == 0 && !local->second.pointeeMsl.isConstexpr
				&& !local->second.constantValue;
		}
		default:
			break;
	}

	if (expression.left && readsRuntimeValue(*expression.left)) {
		return true;
	}
	if (expression.right && readsRuntimeValue(*expression.right)) {
		return true;
	}
	for (const ExpressionPtr& argument: expression.arguments) {
		if (readsRuntimeValue(*argument)) {
			return true;
		}
	}
	for (const InitializerElement& element: expression.elements) {
		if (readsRuntimeValue(*element.value)) {
			return true;
		}
	}
	return false;
}

// A constexpr struct or vector copied into another: its own initializer was
// validated where it was declared.
bool Emitter::namesConstexprAggregate(const Expression& expression) {
	if (expression.kind != ExpressionKind::Identifier) { return false; }
	const auto local = expression.fileScope ? _bindings.end() : _bindings.find(expression.name);
	if (local != _bindings.end()) {
		const Type& type = local->second.pointeeMsl;
		return type.isConstexpr && !type.isPointer && !type.arrayLength && !type.isMatrix()
			&& (type.isVector() || !type.namedType.empty());
	}
	for (const VariableDeclaration& global: _unit.globals) {
		if (global.name == expression.name) {
			return global.type.isConstexpr && !global.type.isPointer && !global.type.arrayLength
				&& !global.type.isMatrix() && (global.type.isVector() || !global.type.namedType.empty());
		}
	}
	return false;
}

// A constexpr brace initializer has to be made of constants. This is checked
// for a declaration that is never reached too, which is invalid all the same.
void Emitter::validateConstexprInitializer(const VariableDeclaration& declaration) {
	if (!declaration.type.isAuto) {
		validateConstexprInitializer(declaration, declaration.type);
	}
}

void Emitter::validateConstexprInitializer(const VariableDeclaration& declaration, const Type& type) {
	if (!type.isConstexpr || !declaration.initializer) {
		return;
	}

	// A matrix has no constexpr constructor in Apple's compiler, so a constexpr
	// matrix is an error whatever it is initialised with.
	if (type.isMatrix() && !type.isPointer && !type.arrayLength) {
		throw CompileError("constexpr variable \"" + declaration.name
			+ "\" must be initialized by a constant expression");
	}

	if (declaration.initializer->kind != ExpressionKind::InitList) {
		const std::string notConstant = "constexpr variable \"" + declaration.name
			+ "\" must be initialized by a constant expression";
		if (readsRuntimeValue(*declaration.initializer)) {
			throw CompileError(notConstant);
		}

		// What can be folded is checked for the operations no constant expression
		// holds, such as a signed overflow or a division by zero. What cannot be
		// folded is left to the lowering.
		if (type.isScalar() && type.namedType.empty()) {
			_foldDomainError = false;
			try {
				foldInitializer(type, *declaration.initializer);
			} catch (const CompileError&) {
				// not a form the folder knows
			}
			const bool outside = _foldDomainError;
			_foldDomainError = false;
			if (outside) {
				throw CompileError(notConstant);
			}
		}
		return;
	}

	const auto validate = [&](const auto& self, const Expression& expression) -> void {
		if (expression.kind == ExpressionKind::InitList) {
			for (const InitializerElement& element: expression.elements) { self(self, *element.value); }
		} else if (!canFoldExpression(expression, declaration.name) && !namesConstexprAggregate(expression)) {
			throw CompileError("a constexpr brace initializer requires a supported constant expression; this form is not lowered yet");
		}
	};
	validate(validate, *declaration.initializer);
}

// Which unscoped enum an expression has the type of. Apple converts neither an
// integer nor another enum to an enum, so a store has to be of the target's own.
// A name for "known not to be an enum" is the empty string; an expression whose
// type is not worked out here is unknown, and a caller then accepts it.
std::optional<std::string> Emitter::enumOf(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::IntLiteral: return expression.enumName;
		case ExpressionKind::FloatLiteral:
		case ExpressionKind::BoolLiteral:
		case ExpressionKind::Binary:
		case ExpressionKind::Sizeof:
			return std::string();
		case ExpressionKind::Assign:
			return enumOf(*expression.left);
		case ExpressionKind::Unary:
			// Arithmetic on an enum is an int. ++ and -- keep the operand's type but
			// are not allowed on an enum at all.
			if (isStep(expression.unaryOperator)) {
				return std::nullopt;
			}
			// The parser builds -1 for a negative enumerator and tags it; a minus written
			// in the source in front of an enumerator is arithmetic and is not.
			return std::optional<std::string>(expression.enumName);
		case ExpressionKind::Identifier: {
			const auto local = expression.fileScope ? _bindings.end() : _bindings.find(expression.name);
			if (local != _bindings.end()) {
				const Type& type = local->second.pointeeMsl;
				if (type.isPointer || type.arrayLength) {
					return std::nullopt;
				}
				return type.enumName;
			}
			for (const VariableDeclaration& global: _unit.globals) {
				if (global.name == expression.name) {
					return global.type.isPointer || global.type.arrayLength
						? std::nullopt : std::optional<std::string>(global.type.enumName);
				}
			}
			return std::nullopt;
		}
		case ExpressionKind::Member: {
			const StructDecl* decl = structOf(*expression.left);
			if (!decl) {
				return std::string();
			}
			std::string declName;
			const size_t field = fieldIndexOf(*decl, expression.memberName, declName);
			if (field == decl->fields.size()) {
				return std::nullopt;
			}
			const Type& type = decl->fields[field].type;
			return type.isPointer || type.arrayLength ? std::nullopt
				: std::optional<std::string>(type.enumName);
		}
		case ExpressionKind::Comma: return enumOf(*expression.right);
		case ExpressionKind::Construct:
			return expression.constructType ? std::optional<std::string>(expression.constructType->enumName)
				: std::nullopt;
		case ExpressionKind::Conditional: {
			// The arms are the value: the same enum in both is that enum, an arm that is
			// known not to be one makes the whole an integer, and two different named
			// enums undergo the usual arithmetic conversions to int.
			const auto first = enumOf(*expression.arguments.at(0));
			const auto second = enumOf(*expression.arguments.at(1));
			if (first && second && *first == *second) {
				return first;
			}
			if ((first && first->empty()) || (second && second->empty())) {
				return std::string();
			}
			if (first && second && *first != "<anonymous>" && *second != "<anonymous>") {
				return std::string();
			}
			return std::nullopt;
		}
		case ExpressionKind::Call: {
			const Type* returnedType = nullptr;
			if (expression.left->kind == ExpressionKind::Identifier) {
				returnedType = returningType(expression, [](const Type& type) {
					return type.isPointer || type.arrayLength ? std::string("*") : type.enumName;
				});
			} else if (expression.left->kind == ExpressionKind::Member) {
				const StructDecl* owner = structOf(*expression.left->left);
				const auto found = owner ? _helpers.find(owner->name + "::" + expression.left->memberName)
					: _helpers.end();
				returnedType = found == _helpers.end() ? nullptr : &found->second.definition->returnType;
				if (owner && found == _helpers.end()) {
					for (const FunctionTemplate* declaration:
						memberTemplatesOf(owner->name, expression.left->memberName)) {
						std::vector<TemplateArgument> arguments;
						const FunctionDecl* pattern = templatePattern(*declaration, expression.left->templateArguments, arguments);
						if (!pattern || pattern->parameters.size() != expression.arguments.size() + 1) {
							continue;
						}
						if (returnedType && returnedType->enumName != pattern->returnType.enumName) {
							return std::nullopt;
						}
						returnedType = &pattern->returnType;
					}
				}
			}
			if (!returnedType) {
				return std::nullopt;
			}
			const Type& returned = *returnedType;
			return returned.isPointer || returned.arrayLength ? std::nullopt
				: std::optional<std::string>(returned.enumName);
		}
		case ExpressionKind::Index: {
			if (const StructDecl* owner = indexOperatorOwner(expression)) {
				const HelperFunction* op = indexOperatorHelper(*owner);
				if (!op) { return std::nullopt; }
				const Type& returned = op->definition->returnType;
				return returned.isPointer || returned.arrayLength ? std::nullopt
					: std::optional<std::string>(returned.enumName);
			}
			if (const Type* pointer = pointerMemberType(*expression.left)) {
				return pointer->enumName;
			}
			if (expression.left->kind == ExpressionKind::Identifier) {
				const auto& table = expression.left->fileScope ? _fileScopeBound : _bindings;
				const auto local = table.find(expression.left->name);
				if (local != table.end() && local->second.pointeeMsl.innerArrayLengths.empty()
					&& (!local->second.isBuffer || !local->second.pointeeMsl.arrayLength)) {
					return local->second.pointeeMsl.enumName;
				}
			}
			if (const std::optional<Type> resolved = placeType(expression);
				resolved && !resolved->isPointer && !resolved->arrayLength) {
				return resolved->enumName;
			}
			return std::nullopt;
		}
		default:
			return std::nullopt;
	}
}

bool Emitter::isComponentValue(const Expression& expression) {
	const auto tag = enumOf(expression);
	return tag && *tag == "metal::component";
}

void Emitter::checkBinaryComponentOperands(BinaryOperator op, const Expression& left, const Expression& right) {
	const bool leftComp = isComponentValue(left);
	const bool rightComp = isComponentValue(right);
	if ((leftComp || rightComp) && !(isComparisonOperator(op) && leftComp && rightComp)) {
		throw CompileError(std::string("operator ") + binaryOperatorSpelling(op)
			+ " does not accept a value of scoped enum metal::component without a cast");
	}
}

void Emitter::checkConditionalOperands(const Expression& condition, const Expression& whenTrue,
	const Expression& whenFalse) {
	requireEnumValue("", condition, "the condition of ?:");
	if (isComponentValue(whenTrue) != isComponentValue(whenFalse)) {
		throw CompileError("conditional operator '?:' cannot mix scoped enum metal::component with another type");
	}
}

Id Emitter::emitNonComponent(const Expression& expression, const std::string& what) {
	requireEnumValue("", expression, what);
	return emitExpression(expression);
}

// The error for a store of something that is not the target's enum. An
// enumerator of an anonymous enum can not be told from the target's here, so it passes.
void Emitter::requireEnumValue(const std::string& target, const Expression& value,
	const std::string& what) {
	const auto given = enumOf(value);
	if (target.empty()) {
		if (given && *given == "metal::component") {
			throw CompileError(what + " does not accept a value of scoped enum metal::component without a cast");
		}
		return;
	}
	if (!given || *given == target || *given == "<anonymous>") {
		return;
	}
	throw CompileError(what + " has the enum type " + target + ", and "
		+ (given->empty() ? std::string("an integer") : "a value of the enum " + *given)
		+ " does not convert to it; write a cast, as (" + target + ")value");
}

// The SPIR-V type of an array type, one array per dimension, outermost first.
Id Emitter::arrayTypeOf(const Type& type) {
	Type base = type;
	base.arrayLength.reset();
	base.innerArrayLengths.clear();
	Id id = declaredTypeOf(base);
	for (auto inner = type.innerArrayLengths.rbegin(); inner != type.innerArrayLengths.rend(); ++inner) {
		id = _types.array(id, *inner);
	}
	return _types.array(id, *type.arrayLength);
}

// The value of an array from a (possibly nested) list. A row that is not in braces takes
// the following entries, as in C++ brace elision; a shorter list zero-fills the rest. The
// list is consumed from position, and a braced list that leaves entries over is an error.
Id Emitter::emitArrayInit(const Type& type, const std::vector<InitializerElement>& entries,
	size_t& position, bool braced, const ArrayLeaves& leaves) {
	// array<T, N> a = {{1, 2, 3}} or {array<T, N>{{1, 2, 3}}}: peel any matching array<T, N>{...}
	// copy-list-init wrapper first, then unwrap an untyped wrapper at most once per dimension.
	const std::vector<InitializerElement>* list = &entries;
	while (type.isStdArray && braced && list->size() == 1 && (*list)[0].fieldName.empty()
		&& (*list)[0].value->kind == ExpressionKind::InitList && (*list)[0].value->constructType
		&& sameArrayType(type, *(*list)[0].value->constructType)) {
		list = &(*list)[0].value->elements;
	}
	if (type.isStdArray && braced && list->size() == 1 && (*list)[0].fieldName.empty()
		&& (*list)[0].value->kind == ExpressionKind::InitList && !(*list)[0].value->constructType) {
		list = &(*list)[0].value->elements;
	}
	Type element = type;
	element.dropOuterDimension();
	const bool rows = element.arrayLength.has_value();
	const uint32_t length = *type.arrayLength;

	std::vector<uint32_t> parts;
	while (parts.size() < length && position < list->size()) {
		const InitializerElement& entry = (*list)[position];
		if (!entry.fieldName.empty()) {
			throw CompileError("a field designator requires a struct initializer");
		}
		const bool nonArrayTypedList = entry.value->kind == ExpressionKind::InitList
			&& entry.value->constructType && entry.value->constructType->arrayRank() < element.arrayRank();
		if (rows && (entry.value->kind != ExpressionKind::InitList || nonArrayTypedList)) {
			parts.push_back(emitArrayInit(element, *list, position, false, leaves));
			continue;
		}
		++position;
		if (rows) {
			requireArrayElementMatch(element, *entry.value);
			size_t inner = 0;
			parts.push_back(emitArrayInit(element, entry.value->elements, inner, true, leaves));
		} else {
			requireArrayElementMatch(element, *entry.value);
			parts.push_back(leaves.element(element, entry.value.get()));
		}
	}
	if (braced && position < list->size()) {
		throw CompileError("excess elements in array initializer");
	}
	while (parts.size() < length) {
		if (rows) {
			size_t none = 0;
			parts.push_back(emitArrayInit(element, {}, none, true, leaves));
		} else {
			parts.push_back(leaves.element(element, nullptr));
		}
	}
	return leaves.compose(arrayTypeOf(type), parts);
}

// The value of an array built from a braced list, in the function's own storage.
Id Emitter::emitArrayListValue(const Type& type, const Expression& list) {
	requireArrayElementMatch(type, list);
	Expression none;
	none.kind = ExpressionKind::InitList;
	ArrayLeaves leaves;
	leaves.element = [&](const Type& leaf, const Expression* entry) {
		if (!entry) {
			return emitInitListValue(leaf, none);
		}
		if (entry->kind == ExpressionKind::InitList) {
			return emitInitListValue(leaf, *entry);
		}
		if (!leaf.isPointer && !leaf.arrayLength) {
			requireEnumValue(leaf.enumName, *entry, "an element of the initializer list");
		}
		const Id value = emitExpression(*entry);
		return leaf.namedType.empty() && leaf.isScalar()
			? convertListScalar(value, leaf, *entry)
			: convertImplicit(value, declaredTypeOf(leaf));
	};
	leaves.compose = [&](Id composite, const std::vector<uint32_t>& parts) {
		return composeStruct(composite, parts);
	};
	size_t position = 0;
	return emitArrayInit(type, list.elements, position, true, leaves);
}

// A fixed-length array in the function's own storage: float a[4], float a[2][3],
// const float3 d[3] = {...} and a[] = {...}. A shorter list zero-fills the rest, as in
// C++, and a longer one is an error. An index is not checked at run time, and a
// constant one past the end is refused.
void Emitter::emitLocalArray(const VariableDeclaration& declaration, const Type& type, bool storageOnly) {
	// A threadgroup array is shared by every invocation of the workgroup, so it
	// lives in Workgroup storage rather than the function's own. Nothing else
	// changes: the type, the initialiser and the index paths are the same, which
	// is why every path that walks a local array has to accept this storage
	// class too (see isLocalStorage).
	//
	// No explicit layout is emitted on the array type. Workgroup storage has
	// an implicit one, and VUID-StandaloneSpirv-None-10684 rejects an explicit
	// one on it, naming the decoration it found: "the Workgroup storage class
	// has an explicit layout from the ArrayStride decoration". The rule is
	// about explicit layout generally, not ArrayStride alone; the same VUID
	// rejects Offset on a struct reached from a Workgroup array. mslc emits
	// neither here, so ArrayStride is the only one it could have got wrong.
	//
	// A separate rule, VUID-StandaloneSpirv-None-04645, limits Workgroup
	// storage to the compute execution model, which is why a threadgroup
	// variable is a kernel-only declaration (see checkThreadgroupLocal).
	//
	// An element is reached with OpAccessChain whose result type is a pointer
	// to the element, not to the array.
	const bool threadgroup = type.addressSpace == AddressSpace::Threadgroup;
	if (threadgroup) {
		// The same two rules a threadgroup scalar obeys: a kernel, and no
		// initialiser. An array that skipped these used to reach the SPIR-V
		// writer and come out as a Workgroup variable in a fragment or vertex
		// entry point, which spirv-val rejects, so mslc exited 0 and wrote a
		// module it had just made invalid.
		checkThreadgroupLocal(declaration);
	} else if (type.resource != ResourceKind::None || type.addressSpace != AddressSpace::None) {
		// Thread was dropped by emitValueDeclaration; device and constant are not a local's.
		throw CompileError("an array of " + typeName(type) + " is not lowered yet; mslc lowers an array "
			"of scalars, vectors, matrices and structs in the function's own storage or in "
			"threadgroup storage");
	}
	const spirv::StorageClassValue storage =
		threadgroup ? spirv::StorageClass::Workgroup : spirv::StorageClass::Function;

	Type element = type;
	element.arrayLength.reset();
	element.innerArrayLengths.clear();
	const Id arrayType = arrayTypeOf(type);

	// A struct with a constructor runs it for every element the list does not give, which
	// is not lowered: zero-filling would skip it and read a different value.
	if (!element.namedType.empty()) {
		const StructDecl* decl = _unit.findStruct(element.namedType);
		if (decl && decl->hasConstructors) {
			size_t elements = 1;
			for (const uint32_t length: type.innerArrayLengths) { elements *= length; }
			elements *= *type.arrayLength;
			const size_t given = declaration.initializer ? declaration.initializer->elements.size() : 0;
			if (given < elements || type.arrayRank() > 1) {
				throw CompileError("an array of " + element.namedType + ", which has a constructor, needs a "
					"value for every element; the constructor is not run for the rest yet");
			}
		}
	}

	// A threadgroup array gets no initialising store: Metal leaves threadgroup memory
	// uninitialised and every invocation reaches this declaration, so a store would
	// erase what the others had already written.
	Id initial = InvalidId;
	if (!storageOnly && !threadgroup) {
		if (!declaration.initializer) {
			initial = _types.zero(arrayType);
		} else if (declaration.initializer->kind != ExpressionKind::InitList && type.isStdArray) {
			initial = convertImplicit(emitExpression(*declaration.initializer), arrayType);
		} else if (declaration.initializer->kind != ExpressionKind::InitList) {
			throw CompileError("array initializer must be an initializer list");
		} else {
			initial = emitArrayListValue(type, *declaration.initializer);
		}
	}

	// A threadgroup array was already declared, and added to the entry point's
	// interface, by the reserveThreadgroupLocal pre-pass that runs before
	// OpEntryPoint is emitted. Re-declaring it here would put a second
	// Workgroup variable in the module that no entry point lists.
	Id variable = InvalidId;
	if (threadgroup) {
		const auto reserved = _threadgroupLocals.find(&declaration);
		if (reserved == _threadgroupLocals.end()) {
			throw CompileError("the threadgroup array \"" + declaration.name
				+ "\" was not reserved before the entry point was emitted");
		}
		variable = reserved->second;
	} else {
		variable = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(storage, arrayType),
			{ static_cast<uint32_t>(storage) });
	}
	if (!storageOnly && !threadgroup) {
		_builder.emit(spirv::OpStore, { variable, initial });
	}
	bindLocal(declaration.name, variable, arrayType, type, storage);
}

void Emitter::emitVariableDeclaration(const VariableDeclaration& declaration, bool storageOnly) {
	if (declaration.type.isAuto) {
		if (storageOnly) {
			throw CompileError("the auto variable \"" + declaration.name + "\" is declared where its "
				"initialiser is not evaluated, so its type cannot be deduced; this is not lowered yet");
		}
		if (declaration.isReference) {
			emitReferenceDeclaration(declaration);
		} else {
			emitAutoDeclaration(declaration);
		}
		return;
	}

	emitValueDeclaration(declaration, declaration.type, InvalidId, storageOnly);
}

// The type an auto takes from the value it is initialised with, keeping what the
// declaration wrote around the auto: const, constexpr and the address space.
Type Emitter::deduceAutoType(const Type& written, Id valueType, const std::string& name) {
	Type deduced;
	deduced.isConst = written.isConst;
	deduced.isConstexpr = written.isConstexpr;
	deduced.isStatic = written.isStatic;
	deduced.addressSpace = written.addressSpace;

	const std::string quoted = "\"" + name + "\"";
	if (valueType == InvalidId) {
		throw CompileError("variable " + quoted + " has incomplete type 'void'");
	}
	if (const auto scalar = _types.scalarKindOf(valueType)) {
		deduced.scalar = *scalar;
		return deduced;
	}
	if (const TypeTable::MatrixInfo* matrix = _types.matrixInfo(valueType)) {
		deduced.scalar = matrix->scalar;
		deduced.matrixColumns = matrix->columns;
		deduced.vectorWidth = matrix->rows;
		return deduced;
	}
	if (const Id component = _types.componentOf(valueType); component != InvalidId) {
		deduced.scalar = *_types.scalarKindOf(component);
		deduced.vectorWidth = _types.vectorWidth(valueType);
		return deduced;
	}
	TypeTable::StructForm form;
	if (_types.structForm(valueType, form)) {
		deduced.namedType = form.name;
		return deduced;
	}
	throw CompileError("the type of " + quoted + " cannot be deduced from its initialiser: it is an array, "
		"a pointer, a texture or another type that is not lowered as a local value yet");
}

// An auto local: the initialiser is evaluated first, and the variable takes the
// type of the value it produced. The name may not appear in the initialiser.
void Emitter::emitAutoDeclaration(const VariableDeclaration& declaration) {
	const Expression& initializer = *declaration.initializer;
	if (initializer.kind == ExpressionKind::InitList && !initializer.constructType) {
		throw CompileError("the auto variable \"" + declaration.name + "\" initialised from a braced "
			"list is not lowered yet");
	}
	if (const auto given = enumOf(initializer); given && !given->empty()) {
		throw CompileError("the auto variable \"" + declaration.name + "\" would have the enum type "
			+ *given + ", which is not lowered yet; write the enum type");
	}

	std::set<std::string> names;
	collectIdentifiers(initializer, names, false);
	if (names.count(declaration.name)) {
		throw CompileError("variable \"" + declaration.name + "\" declared with deduced type 'auto' "
			"cannot appear in its own initializer");
	}

	if (initializer.kind == ExpressionKind::Identifier) {
		const Binding* named = findResourceBinding(initializer.name, initializer.fileScope);
		if (named && named->pointeeMsl.resource != ResourceKind::None) {
			bindResourceAlias(declaration, nullptr);
			return;
		}
	}

	const Id value = emitExpression(initializer);
	const Type type = deduceAutoType(declaration.type, _builder.typeOf(value), declaration.name);
	emitValueDeclaration(declaration, type, value, false);
}

// A local of value type. deduced is the already evaluated initialiser of an
// auto, or InvalidId when the declaration's own initialiser is evaluated here.
void Emitter::emitValueDeclaration(const VariableDeclaration& declaration, const Type& type, Id deduced,
	bool storageOnly) {
	if (type.resource != ResourceKind::None) {
		if (declaration.initializer) {
			bindResourceAlias(declaration, &type);
		} else {
			declareLocalSampler(declaration);
		}
		return;
	}

	// "thread float3 v", "thread S s = {..}" and "thread float a[4]" are the function's own
	// storage, which is what an unqualified local is, so the qualifier is dropped here once
	// for every value local. A pointer or a reference keeps it: the address space is part of
	// what it points at.
	if (type.addressSpace == AddressSpace::Thread && !type.isPointer && !declaration.isReference) {
		Type plain = type;
		plain.addressSpace = AddressSpace::None;
		emitValueDeclaration(declaration, plain, deduced, storageOnly);
		return;
	}

	if (type.isAtomic && !type.isPointer
		&& (type.addressSpace != AddressSpace::Threadgroup || type.arrayLength || declaration.isReference)) {
		throw CompileError("a local atomic \"" + declaration.name + "\" is not allowed: an atomic lives in a "
			"device buffer, and a function takes a pointer to it");
	}

	if (declaration.isReference) {
		emitReferenceDeclaration(declaration);
		return;
	}

	// A local has to be a value: the declaration stores an initialiser into a
	// Function-storage variable, and declaredTypeOf answers a pointer type with
	// its pointee, so a local of pointer or array type would be declared as that
	// base and have the initialiser converted to it. "float *p = 0;" stored a
	// float 0.0 into a pointer and "float[2] v;" declared one float, both of
	// which validate. The type is quoted as the source spelled it, so the
	// message names "float *" rather than "float".
	if (type.isPointer) {
		throw CompileError("a local of type " + typeName(type)
			+ " is not lowered yet, since a pointer is not a value mslc can declare "
				"in a function's own storage");
	}

	if (type.arrayLength) {
		emitLocalArray(declaration, type, storageOnly);
		return;
	}

	const Id typeId = declaredTypeOf(type);

	if (type.addressSpace == AddressSpace::Threadgroup) {
		declareThreadgroupLocal(declaration, type, typeId);
		return;
	}

	if (!storageOnly) {
		validateConstexprInitializer(declaration, type);
	}
	if (!storageOnly && declaration.initializer
		&& declaration.initializer->kind != ExpressionKind::InitList
		&& (!type.enumName.empty() || (!type.isPointer && !type.arrayLength))) {
		requireEnumValue(type.enumName, *declaration.initializer,
			"the variable \"" + declaration.name + "\"");
	}

	// The name is in scope in its own initializer, so the binding is installed
	// before the initializer is looked up rather than after. "int value = value;"
	// resolves the int being declared, whose slot is still uninitialized, which
	// is what Apple accepts with only an uninitialized warning. Binding after
	// the initializer resolved the outer name instead: a file-scope float4 named
	// value made the initializer a scalar and the conversion was rejected with
	// "a scalar cannot be converted to a vector".
	const Id pointerType = _types.pointer(spirv::StorageClass::Function, typeId);
	const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
		{ static_cast<uint32_t>(spirv::StorageClass::Function) });
	bindLocal(declaration.name, id, typeId, type);

	Id initial = InvalidId;
	if (deduced != InvalidId) {
		initial = convertImplicit(deduced, typeId);
	} else if (!storageOnly && declaration.initializer) {
		const Id initializer = declaration.initializer->kind == ExpressionKind::InitList
			? emitInitListValue(type, *declaration.initializer)
			: emitExpression(*declaration.initializer);
		initial = convertImplicit(initializer, typeId);
	} else if (!storageOnly) {
		initial = _types.zero(typeId);
	}

	if (!storageOnly) {
		_builder.emit(spirv::OpStore, { id, initial });
	}

	const Type& declared = type;
	if (declaration.initializer && declared.isConst && !declared.isConstexpr && !declared.isPointer
		&& !declared.isVector() && !declared.isMatrix() && declared.namedType.empty()
		&& !declared.arrayLength && !isFloatKind(declared.scalar)
		&& !readsRuntimeValue(*declaration.initializer)) {
		_bindings[declaration.name].constantValue = true;
	}
	if (!storageOnly && declaration.initializer && !_types.isAggregate(typeId)
		&& (type.isConstexpr || (type.isConst && !isFloatKind(type.scalar)))) {
		const Expression* expression = declaration.initializer.get();
		while (expression->kind == ExpressionKind::InitList && !expression->constructType
			&& expression->elements.size() == 1 && expression->elements.front().fieldName.empty()) {
			expression = expression->elements.front().value.get();
		}
		FoldedConstant folded;
		folded.scalar = type.scalar;
		const bool empty = expression->kind == ExpressionKind::InitList && !expression->constructType
			&& expression->elements.empty();
		if (empty || canFoldExpression(*expression, declaration.name)) {
			if (!empty) { folded = foldExpression(*expression); }
			const ScalarKind to = type.scalar;
			if (to == ScalarKind::Bool) {
				folded.boolean = folded.scalar == ScalarKind::Bool ? folded.boolean
					: isFloatKind(folded.scalar) ? folded.number != 0 : folded.integer != 0;
				folded.scalar = to;
			} else if (folded.scalar == ScalarKind::Bool) {
				folded.integer = folded.boolean ? 1 : 0;
				folded.scalar = ScalarKind::Int;
			}
			if (!isFloatKind(folded.scalar) || isFloatKind(to)) {
				const FoldedConstant converted = convertConstant(folded, to);
				if (!isFloatKind(to) || std::isfinite(converted.number)) { _localFolded[id] = converted; }
			}
		}
	}
	if (!storageOnly && type.isConst && declaration.initializer
		&& type.namedType.empty() && type.isScalar()
		&& !isFloatKind(type.scalar)) {
		try {
			_bindings[declaration.name].folded = foldInitializer(type, *declaration.initializer);
		} catch (const CompileError&) {
			// A const local may have a runtime initializer without being a constant expression.
		}
	}
}

void Emitter::declareThreadgroupLocal(const VariableDeclaration& declaration, const Type& type, Id typeId) {
	checkThreadgroupLocal(declaration);
	const auto reserved = _threadgroupLocals.find(&declaration);
	if (reserved == _threadgroupLocals.end()) {
		throw CompileError("the threadgroup local \"" + declaration.name + "\" is not lowered yet");
	}
	bindLocal(declaration.name, reserved->second, typeId, type);
	_bindings[declaration.name].storageClass = spirv::StorageClass::Workgroup;
}

// pointeeMsl is what a member access on the local resolves its fields against.
void Emitter::bindLocal(const std::string& name, Id variable, Id type, const Type& msl,
	spirv::StorageClassValue storage) {
	Binding binding;
	binding.id = variable;
	binding.isPointer = true;
	binding.pointeeType = type;
	binding.storageClass = storage;
	binding.pointeeMsl = msl;
	binding.readOnly = msl.isConst;
	_bindings[name] = binding;
}

bool Emitter::isLocalStorage(spirv::StorageClassValue storage) const {
	return storage == spirv::StorageClass::Function
		|| storage == spirv::StorageClass::Private
		|| storage == spirv::StorageClass::Workgroup;
}

// A threadgroup variable is one Workgroup variable at module scope, shared by every
// invocation of the group. Apple takes it only in a kernel. mslc refuses an
// initialiser: Apple initialises it, but how many invocations run the
// initialiser and in what order is not something the source states.
void Emitter::checkThreadgroupLocal(const VariableDeclaration& declaration) {
	if (_helper || _entryPoint->stage != Stage::Kernel) {
		throw CompileError("variables in the threadgroup address space can only be declared in a kernel function");
	}
	if (declaration.initializer) {
		throw CompileError("a threadgroup local \"" + declaration.name + "\" with an initialiser is not lowered "
			"yet; declare it without one and assign it after a threadgroup_barrier");
	}
}

} // namespace mslc
