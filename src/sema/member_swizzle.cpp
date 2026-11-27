#include "sema/emitter.h"

namespace mslc {

size_t fieldIndexOf(const StructDecl& decl, const std::string& member, std::string& outDeclName) {
	for (size_t i = 0; i < decl.fields.size(); ++i) {
		if (decl.fields[i].name == member) {
			outDeclName = decl.name;
			return i;
		}
	}

	outDeclName = decl.name;
	return decl.fields.size();
}

// The type of a device or constant pointer member that a member access names,
// as p in s.p; null for any other expression.
const Type* Emitter::pointerMemberType(const Expression& expression) {
	if (expression.kind != ExpressionKind::Member) {
		return nullptr;
	}
	const StructDecl* decl = structOf(*expression.left);
	if (!decl) {
		return nullptr;
	}
	std::string declName;
	const size_t field = fieldIndexOf(*decl, expression.memberName, declName);
	return field < decl->fields.size() && decl->fields[field].type.isPointer
		? &decl->fields[field].type : nullptr;
}

// s.p as the pointer it holds, bound as a helper's pointer parameter is, so that
// s.p[i] and *s.p walk the buffer it points into. Nothing for an expression that
// is not a pointer member.
std::optional<Binding> Emitter::pointerMemberBinding(const Expression& expression) {
	const Type* type = pointerMemberType(expression);
	if (!type) {
		return std::nullopt;
	}

	Id fieldType = InvalidId;
	const Id address = emitMemberAddress(expression, fieldType);
	const auto [pointee, block] = bufferParamTypes(*type);
	Binding bound;
	bound.id = loadFromBuffer(address, fieldType);
	bound.isPointer = true;
	bound.pointeeType = pointee;
	bound.bufferPointeeType = block;
	bound.storageClass = spirv::StorageClass::PhysicalStorageBuffer;
	bound.isBuffer = true;
	bound.ownAddress = true;
	bound.readOnly = type->isConst || type->addressSpace == AddressSpace::Constant;
	bound.pointeeMsl = *type;
	bound.pointeeMsl.isPointer = false;
	bound.pointeeMsl.isConst = false;
	return bound;
}

const ConstantBinding* Emitter::constantFor(const Expression* expression) {
	if (!expression || expression->kind != ExpressionKind::Identifier) {
		return nullptr;
	}

	// An entry point's own name wins unless qualified with "::": a parameter may be called the same as
	// a constant, and then it is the parameter the source means.
	if (!expression->fileScope && _bindings.count(expression->name) > 0) {
		return nullptr;
	}

	const auto it = _constants.find(expression->name);
	return it == _constants.end() ? nullptr : &it->second;
}

const Expression* accessChain(const Expression& expression, std::string& outName,
	std::vector<AccessStep>& outSteps) {

	std::vector<AccessStep> reversed;

	const Expression* current = &expression;
	while (current->kind == ExpressionKind::Member || current->kind == ExpressionKind::Index) {
		AccessStep step;
		step.isIndex = current->kind == ExpressionKind::Index;

		if (step.isIndex) {
			step.isDereference = current->isDereference;
			if (step.isDereference && current->left->kind != ExpressionKind::Identifier) {
				throw CompileError(kDereferenceOperand);
			}

			if (current->arguments.size() != 1) {
				throw CompileError("expected exactly one index, found "
					+ std::to_string(current->arguments.size()));
			}

			step.index = current->arguments[0].get();
		} else {
			step.memberName = current->memberName;
			step.isArrow = current->isArrow;
		}

		reversed.push_back(step);
		current = current->left.get();
	}

	if (current->kind != ExpressionKind::Identifier) {
		throw CompileError("a member or an index has to start at a name mslc knows");
	}

	outName = current->name;
	outSteps.assign(reversed.rbegin(), reversed.rend());
	return current;
}


const StructDecl* Emitter::structOf(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::Identifier: {
			const auto& table = expression.fileScope ? _fileScopeBound : _bindings;
			const auto it = table.find(expression.name);
			if (it != table.end()) {
				const std::string& name = it->second.pointeeMsl.namedType;
				return name.empty() ? nullptr : _unit.findStruct(name);
			}
			const auto constant = _constants.find(expression.name);
			return constant == _constants.end() ? nullptr : constant->second.structType;
		}

		case ExpressionKind::Member: {
			const StructDecl* parent = structOf(*expression.left);
			if (!parent) {
				return nullptr;
			}
			std::string name;
			const size_t field = fieldIndexOf(*parent, expression.memberName, name);
			return field == parent->fields.size() ? nullptr
				: _unit.findStruct(parent->fields[field].type.namedType);
		}

		// An element of a buffer has the buffer's own type; a struct's operator[] yields its return type.
		case ExpressionKind::Index: {
			if (const StructDecl* owner = indexOperatorOwner(expression)) {
				const HelperFunction* op = indexOperatorHelper(*owner);
				const Type* returned = op ? &op->definition->returnType : nullptr;
				return !returned || returned->namedType.empty() || returned->isPointer || returned->arrayLength
					? nullptr : _unit.findStruct(returned->namedType);
			}
			if (std::optional<Type> arr = arrayExpressionType(*expression.left)) {
				arr->dropOuterDimension();
				return arr->arrayLength || arr->isPointer || arr->namedType.empty()
					? nullptr : _unit.findStruct(arr->namedType);
			}
			return structOf(*expression.left);
		}

		// Either arm of c ? a : b names the struct both arms share.
		case ExpressionKind::Conditional: {
			const StructDecl* taken = structOf(*expression.arguments[0]);
			return taken ? taken : structOf(*expression.arguments[1]);
		}

		case ExpressionKind::Comma: return structOf(*expression.right);

		// S{...} is a value of the struct it names.
		case ExpressionKind::InitList:
			return expression.constructType && !expression.constructType->arrayLength
				&& !expression.constructType->namedType.empty()
				? _unit.findStruct(expression.constructType->namedType) : nullptr;

		// A helper that returns a struct, including a struct's constructor, which
		// is one.
		case ExpressionKind::Call: {
			// A lookup by name only: findHelper evaluates a receiver it does not know
			// the type of, and this has to leave no code behind.
			const Type* returnedType = nullptr;
			if (expression.left->kind == ExpressionKind::Identifier) {
				returnedType = returningType(expression, [](const Type& type) {
					return type.isPointer || type.arrayLength ? std::string("*") : type.namedType;
				});
			} else if (expression.left->kind == ExpressionKind::Member) {
				const StructDecl* owner = structOf(*expression.left->left);
				const auto found = owner ? _helpers.find(owner->name + "::" + expression.left->memberName)
					: _helpers.end();
				returnedType = found == _helpers.end() ? nullptr : &found->second.definition->returnType;
				if (owner && found == _helpers.end()) {
					// A member function template returns a struct known once its arguments are.
					for (const FunctionTemplate* declaration:
						memberTemplatesOf(owner->name, expression.left->memberName)) {
						std::vector<TemplateArgument> arguments;
						const FunctionDecl* pattern = templatePattern(*declaration, expression.left->templateArguments, arguments);
						if (!pattern || pattern->parameters.size() != expression.arguments.size() + 1) {
							continue;
						}
						// A result that is a template parameter is no struct here until deduced, as for a
						// function template (f(s).field is refused there too).
						if (returnedType && returnedType->namedType != pattern->returnType.namedType) {
							throw CompileError("the overloads of the member function template \"" + declaration->name
								+ "\" differ in the struct they return, and which one the call chooses is not known "
								"before its arguments are evaluated; assign the call to a variable first");
						}
						returnedType = &pattern->returnType;
					}
				}
			}
			if (!returnedType) {
				return nullptr;
			}
			const Type& returned = *returnedType;
			return returned.namedType.empty() || returned.isPointer || returned.arrayLength
				? nullptr : _unit.findStruct(returned.namedType);
		}

		default: return nullptr;
	}
}

// The lane each letter of a swizzle names, from one of the two letter sets
// Metal allows. Shared by a read and a store, so both reject the same spellings.
std::vector<uint32_t> swizzleLanes(const std::string& name) {
	const std::string quoted = "\"." + name + "\"";
	if (name.size() > 4) {
		throw CompileError(quoted + " names more than four components");
	}

	std::vector<uint32_t> indices;
	const std::string_view set = std::string_view("xyzw").find(name[0]) != std::string_view::npos
		? "xyzw" : "rgba";
	for (const char letter: name) {
		const size_t index = set.find(letter);
		if (index == std::string_view::npos) {
			throw CompileError(quoted + " is not a swizzle: each letter has to be one of "
				"xyzw, or each one of rgba");
		}
		indices.push_back(static_cast<uint32_t>(index));
	}

	return indices;
}

void requireLanesWithin(const std::string& name, const std::vector<uint32_t>& lanes, uint32_t width) {
	for (const uint32_t lane: lanes) {
		if (lane >= width) {
			throw CompileError("\"." + name + "\" names a component past the end of a vector of "
				+ std::to_string(width));
		}
	}
}

// v.zyx, (a * b).xyz, normalize(n).w: the components of a vector value, in
// the order named, from one of the two letter sets Metal allows.
Id Emitter::emitSwizzle(const Expression& expression) {
	const std::string& name = expression.memberName;
	std::vector<uint32_t> indices;
	try {
		indices = swizzleLanes(name);
	} catch (const CompileError&) {
		if (expression.left->kind == ExpressionKind::Call && callsDependentTemplate(*expression.left)) {
			throw CompileError("\"." + name + "\" is read from the result of a function template call whose type "
				"is known only once it is deduced; assign the call to a variable first");
		}
		throw;
	}

	const Id vector = emitExpression(*expression.left);
	const Id type = _builder.typeOf(vector);
	if (_types.componentOf(type) == InvalidId) {
		throw CompileError("\"." + name + "\" swizzles a value that is not a vector");
	}

	requireLanesWithin(name, indices, _types.vectorWidth(type));

	if (indices.size() == 1) {
		return _builder.emitTyped(spirv::OpCompositeExtract, _types.componentOf(type),
			{ vector, indices[0] });
	}

	std::vector<uint32_t> operands = { vector, vector };
	operands.insert(operands.end(), indices.begin(), indices.end());
	return _builder.emitTyped(spirv::OpVectorShuffle,
		_types.withWidth(type, static_cast<uint32_t>(indices.size())), operands);
}

// The array member s.field[i][j] names, when the base of this index chain is a
// member of a struct and the field is an array: its rank, else zero.
size_t Emitter::arrayMemberRank(const Expression& expression, size_t& outIndices) {
	outIndices = 0;
	const Expression* base = &expression;
	while (base->kind == ExpressionKind::Index) {
		++outIndices;
		base = base->left.get();
	}
	if (base->kind != ExpressionKind::Member) {
		return 0;
	}
	const StructDecl* decl = structOf(*base->left);
	if (!decl) {
		return 0;
	}
	std::string declName;
	const size_t field = fieldIndexOf(*decl, base->memberName, declName);
	return field == decl->fields.size() ? 0 : decl->fields[field].type.arrayRank();
}

// s.field[i]: the index is a step of the member's own access chain. With more
// indices than the member has dimensions, the extra ones index its element,
// and the inner expression arrives here with exactly the dimensions. Nothing
// when the expression is not an index into an array member.
std::optional<Id> Emitter::emitArrayMemberElement(const Expression& expression, bool asAddress) {
	size_t indices = 0;
	const size_t rank = arrayMemberRank(expression, indices);
	if (rank == 0 || indices > rank) {
		return std::nullopt;
	}
	const Expression* base = &expression;
	while (base->kind == ExpressionKind::Index) {
		base = base->left.get();
	}
	if (indices < rank) {
		const std::optional<Type> member = placeType(*base);
		if (!member || !member->isStdArray) {
			throw CompileError("an array member indexed this far is still an array, which is not a value: "
				"index it down to an element");
		}
	}
	if (!asAddress && isComputedValue(*base->left)) return std::nullopt;
	Id fieldType = InvalidId;
	const Id address = emitMemberAddress(expression, fieldType);
	if (asAddress) {
		return address;
	}
	std::string rootName;
	std::vector<AccessStep> steps;
	const Expression* rootExpr = accessChain(expression, rootName, steps);
	const Binding* root = findBinding(*rootExpr);
	return root && (root->bufferPointeeType != InvalidId
			|| root->storageClass == spirv::StorageClass::PhysicalStorageBuffer)
		? loadFromBuffer(address, fieldType) : loadFrom(address, fieldType);
}

// A struct that is computed rather than named: a call, S{...}, a conditional or
// comma of them, or a member of one. It has no address to walk, so its members
// are taken from the value.
bool Emitter::isStructValue(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::Call:
		case ExpressionKind::InitList:
		case ExpressionKind::Conditional:
		case ExpressionKind::Comma: return true;
		case ExpressionKind::Index: return isComputedValue(expression) && structOf(expression) != nullptr;
		case ExpressionKind::Member: return isStructValue(*expression.left);
		default: return false;
	}
}

Id Emitter::emitMember(const Expression& expression) {
	if (!structOf(*expression.left)) {
		return emitSwizzle(expression);
	}

	// A member of a value a helper returned has no address; the call is made and
	// the field taken from the struct it produced.
	if (isStructValue(*expression.left)) {
		const StructDecl* decl = structOf(*expression.left);
		std::string declName;
		const size_t field = fieldIndexOf(*decl, expression.memberName, declName);
		if (field == decl->fields.size()) {
			throw CompileError("struct \"" + declName + "\" has no member \""
				+ expression.memberName + "\"");
		}
		const Id value = emitExpression(*expression.left);
		const Type& fieldType = decl->fields[field].type;
		return _builder.emitTyped(spirv::OpCompositeExtract,
			fieldType.arrayLength ? arrayTypeOf(fieldType) : declaredTypeOf(fieldType),
			{ value, static_cast<uint32_t>(field) });
	}

	const ConstantBinding* constant = constantFor(expression.left.get());
	if (constant) {
		std::string declName;
		const size_t field = fieldIndexOf(*constant->structType, expression.memberName, declName);
		if (field == constant->structType->fields.size()) {
			throw CompileError("struct \"" + declName + "\" has no member \""
				+ expression.memberName + "\"");
		}

		// A file-scope constant is a value rather than a place, so there is no
		// address to load through and the field is taken from the value. Every
		// other struct-valued name is a place, so it goes through an address.
		return _builder.emitTyped(spirv::OpCompositeExtract,
			declaredTypeOf(constant->structType->fields[field].type),
			{ constant->id, static_cast<uint32_t>(field) });
	}

	if (pointerMemberType(expression)) {
		throw CompileError("the pointer member \"" + expression.memberName + "\" is only lowered "
			"when indexed or dereferenced, as in s.p[i] or *s.p");
	}

	// The chain says which binding the access starts from, which is not the
	// name on the left of the '.' when that is an index.
	std::string rootName;
	std::vector<AccessStep> steps;
	const Expression* rootExpr = accessChain(expression, rootName, steps);

	Id fieldType = InvalidId;
	const Id address = emitMemberAddress(expression, fieldType);

	// A member read yields the field and a member write needs its address, so
	// the address form does the work and this loads. A buffer's load carries the
	// Aligned operand the same rule as any other access through its pointer.
	const Binding* it = findBinding(*rootExpr);
	return it && (it->bufferPointeeType != InvalidId
			|| it->storageClass == spirv::StorageClass::PhysicalStorageBuffer)
		? loadFromBuffer(address, fieldType)
		: loadFrom(address, fieldType);
}

// The address a member access names, and the type of the value it points at.
//
// A buffer is a wrapper struct holding a runtime array, so an element of one
// is two steps in: into the member, then into the array. A struct reached
// directly is the buffer itself, so its first step is a field index with no
// member in front of it.
Id Emitter::emitMemberAddress(const Expression& expression, Id& outFieldType) {
	std::string rootName;
	std::vector<AccessStep> steps;
	const Expression* rootExpr = accessChain(expression, rootName, steps);

	const Binding* found = findBinding(*rootExpr);
	if (!found) {
		throw CompileError("\"" + rootName + "\" is not a parameter or local");
	}

	const Binding& binding = *found;
	const bool fromBuffer = binding.bufferPointeeType != InvalidId;

	// What the chain points at now, as the source spelled it, so the next step
	// can tell a struct from something else and resolve a field against the
	// right declaration. An index does not change it: a buffer's element is
	// the buffer's own type.
	Type current = binding.pointeeMsl;

	std::vector<uint32_t> operands;
	if (fromBuffer && binding.isBuffer) {
		// Into the runtime array the wrapper holds, before the element index.
		operands.push_back(constantU32(0));

		// The element itself. A source index names it; "->" and a member reached
		// straight off the pointer mean element zero, which is what a pointer to
		// a buffer's first element is. Reading the field without it would index
		// the element struct by a field index and read the wrong bytes.
		if (!steps.empty() && !steps[0].isIndex) {
			operands.push_back(constantU32(0));
		}
	}

	outFieldType = InvalidId;
	bool inMember = false;
	const StructDecl* memberOwner = nullptr;
	size_t memberField = 0;

	for (const AccessStep& step: steps) {
		if (inMember && current.isPointer) {
			throw CompileError("the pointer member \"" + memberOwner->fields[memberField].name
				+ "\" is only lowered when indexed or dereferenced, as in s.p[i] or *s.p");
		}
		if (step.isIndex) {
			// An element of an array member: one more step of the same chain, in
			// whichever storage class the struct lives.
			if (inMember && current.arrayLength) {
				if (const auto literal = literalIndex(*step.index); literal && *literal >= *current.arrayLength) {
					throw CompileError("the index " + std::to_string(*literal) + " is outside the array member of "
						+ std::to_string(*current.arrayLength));
				}
				operands.push_back(stepIndex(*step.index, "array element"));
				current.dropOuterDimension();
				continue;
			}

			// Only a pointer names a buffer, and only a buffer is an array. A
			// struct reached directly is one value, whether through a reference
			// or through the address a buffer of scalars put in a register.
			// A local array is indexed by an access chain step into the array.
			if (!fromBuffer && current.arrayLength && isLocalStorage(binding.storageClass)) {
				if (const auto literal = literalIndex(*step.index); literal && *literal >= *current.arrayLength) {
					throw CompileError("the index " + std::to_string(*literal) + " is outside the array \""
						+ rootName + "\" of " + std::to_string(*current.arrayLength));
				}
				operands.push_back(stepIndex(*step.index, "array element"));
				current.dropOuterDimension();
				continue;
			}

			if (!fromBuffer || !binding.isBuffer) {
				if (step.isDereference) {
					throw CompileError(notAPointer(rootName));
				}
				throw CompileError("\"" + rootName + "\" is not a buffer, so it cannot "
					"be indexed as an array");
			}

			operands.push_back(stepIndex(*step.index, "buffer element"));
			continue;
		}

		// "p->m" on a pointer parameter bound to a local array names element zero.
		if (!fromBuffer && !inMember && current.arrayLength && isLocalStorage(binding.storageClass)) {
			if (!step.isArrow) {
				throw CompileError("\"" + rootName + "\" is an array, so \"." + step.memberName
					+ "\" needs an element: index it, as " + rootName + "[i]." + step.memberName);
			}
			operands.push_back(constantU32(0));
			current.arrayLength.reset();
		}

		const StructDecl* decl = current.namedType.empty()
			? nullptr : _unit.findStruct(current.namedType);
		if (!decl) {
			throw CompileError("\"" + step.memberName + "\" is not a member of \""
				+ rootName + "\", which is not a struct there");
		}

		std::string declName;
		const size_t field = fieldIndexOf(*decl, step.memberName, declName);
		if (field == decl->fields.size()) {
			throw CompileError("struct \"" + declName + "\" has no member \""
				+ step.memberName + "\"");
		}

		operands.push_back(constantU32(static_cast<uint32_t>(field)));
		current = decl->fields[field].type;
		inMember = true;
		memberOwner = decl;
		memberField = field;
	}

	if (current.namedType.empty() && steps.back().isIndex && !inMember) {
		throw CompileError("a member access has to end at a struct field");
	}
	if (current.arrayLength && !current.isStdArray) {
		throw CompileError("an array member is used as a whole value, "
			"which mslc does not lower; index it down to an element");
	}

	outFieldType = declaredTypeOf(current);
	const bool arrayInBuffer = fromBuffer || binding.storageClass == spirv::StorageClass::PhysicalStorageBuffer;
	if ((current.arrayLength || current.isPointer) && arrayInBuffer) {
		// A struct in a buffer holds its array with a stride, which is another type than the
		// local's, and a pointer as a buffer pointer; the member's type in the laid-out
		// struct is what the chain points at.
		TypeTable::StructForm form;
		if (!memberOwner || !_types.structForm(_types.namedStruct(memberOwner->name), form)) {
			throw CompileError("an array member in a buffer has no laid-out struct to take its type from");
		}
		outFieldType = form.declared[memberField];
		for (size_t dimension = memberOwner->fields[memberField].type.arrayRank(); dimension > current.arrayRank(); --dimension) {
			outFieldType = _types.arrayElement(outFieldType);
		}
	}

	const spirv::StorageClassValue storageClass = fromBuffer
		? spirv::StorageClass::PhysicalStorageBuffer : binding.storageClass;

	// The base is the address the chain starts from: the buffer's own pointer,
	// loaded from the binding-0 block, or the variable itself.
	operands.insert(operands.begin(), fromBuffer ? bufferBase(binding) : binding.id);

	// A packed member of a laid-out struct is stored as an array of its
	// components, and is viewed as the vector once it has an address.
	// A reference local bound to a buffer element holds that element's address
	// and has no buffer of its own, but its fields are laid out the same way.
	const bool inBuffer = fromBuffer || binding.storageClass == spirv::StorageClass::PhysicalStorageBuffer;
	const bool packed = inBuffer && current.isPacked && !current.isPointer;
	const bool bytes = inBuffer && _types.isBool(outFieldType);
	const Id address = _builder.emitTyped(spirv::OpAccessChain,
		_types.pointer(storageClass, packed
			? _types.packedStorage(current.scalar, current.vectorWidth)
			: bytes ? _types.boolStorage(outFieldType) : outFieldType), operands);

	return packed ? packedVectorAddress(address, outFieldType)
		: bytes ? boolAddress(address, outFieldType) : address;
}

} // namespace mslc
