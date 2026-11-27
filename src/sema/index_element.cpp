#include "sema/emitter.h"

namespace mslc {

std::string notAPointer(const std::string& name) {
	return "\"" + name + "\" is not a pointer, so unary '*' cannot dereference it";
}


const Binding* Emitter::arrayRootBinding(const Expression& expression, size_t& depth) const {
	depth = 0;
	const Expression* current = &expression;
	while (current->kind == ExpressionKind::Index && !current->isDereference) {
		++depth;
		current = current->left.get();
	}
	if (depth == 0 || current->kind != ExpressionKind::Identifier) {
		return nullptr;
	}
	const Binding* found = findBinding(*current);
	if (!found) {
		return nullptr;
	}
	const Binding& binding = *found;
	const bool ownStorage = isLocalStorage(binding.storageClass);
	return ownStorage && binding.isPointer && !binding.isBuffer && binding.bufferPointeeType == InvalidId
		&& binding.pointeeMsl.arrayRank() >= 2 && depth <= binding.pointeeMsl.arrayRank() ? &binding : nullptr;
}

// a[i][j] on a multi-dimensional array: one access chain with a step per index.
Id Emitter::emitLocalArrayIndex(const Expression& expression, bool asAddress) {
	size_t depth = 0;
	const Binding& binding = *arrayRootBinding(expression, depth);

	std::vector<const Expression*> indices(depth);
	const Expression* current = &expression;
	for (size_t level = depth; level-- > 0; current = current->left.get()) {
		if (current->arguments.size() != 1) {
			throw CompileError("expected exactly one index, found " + std::to_string(current->arguments.size()));
		}
		indices[level] = current->arguments[0].get();
	}

	const std::string& name = current->name;
	Type msl = binding.pointeeMsl;
	Id type = binding.pointeeType;
	std::vector<uint32_t> operands { binding.id };
	for (const Expression* index: indices) {
		if (const auto literal = literalIndex(*index); literal && *literal >= *msl.arrayLength) {
			throw CompileError("the index " + std::to_string(*literal) + " is outside the array \""
				+ name + "\" of " + std::to_string(*msl.arrayLength));
		}
		operands.push_back(stepIndex(*index, "array element"));
		msl.dropOuterDimension();
		type = _types.arrayElement(type);
	}

	if (msl.arrayLength && !msl.isStdArray) {
		throw CompileError("\"" + name + "\" indexed this far is still an array, which is not a value: "
			"index it down to an element");
	}

	const Id address = _builder.emitTyped(spirv::OpAccessChain,
		_types.pointer(binding.storageClass, type), operands);
	return asAddress ? address : loadFrom(address, type);
}

std::optional<Type> Emitter::arrayExpressionType(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::Identifier: {
			const auto& table = expression.fileScope ? _fileScopeBound : _bindings;
			const auto it = table.find(expression.name);
			return it != table.end() && !it->second.isBuffer && it->second.pointeeMsl.arrayLength
				? std::optional<Type>(it->second.pointeeMsl) : std::nullopt;
		}
		case ExpressionKind::Member:
			if (!arrayExpressionType(*expression.left) || expression.isArrow) {
				if (const StructDecl* decl = structOf(*expression.left)) {
					for (const StructField& f: decl->fields) {
						if (f.name == expression.memberName && f.type.arrayLength) return f.type;
					}
				}
			}
			return std::nullopt;
		case ExpressionKind::Index: {
			if (expression.left->kind == ExpressionKind::Identifier && !expression.left->fileScope) {
				const auto it = _bindings.find(expression.left->name);
				if (it != _bindings.end() && it->second.isBuffer) {
					return it->second.pointeeMsl.arrayLength ? std::optional<Type>(it->second.pointeeMsl) : std::nullopt;
				}
			}
			std::optional<Type> base = arrayExpressionType(*expression.left);
			if (base) base->dropOuterDimension();
			return base && base->arrayLength ? base : std::nullopt;
		}
		case ExpressionKind::InitList:
		case ExpressionKind::Construct:
			return expression.constructType && expression.constructType->arrayLength ? expression.constructType : std::nullopt;
		case ExpressionKind::Conditional: {
			const auto yes = arrayExpressionType(*expression.arguments[0]);
			return yes ? yes : arrayExpressionType(*expression.arguments[1]);
		}
		case ExpressionKind::Comma: return arrayExpressionType(*expression.right);
		case ExpressionKind::Call: {
			const Type* returned = nullptr;
			if (expression.left->kind == ExpressionKind::Identifier) {
				returned = returningType(expression, [](const Type& t) { return t.arrayLength && !t.isPointer ? typeName(t) : std::string(); });
			} else if (expression.left->kind == ExpressionKind::Member) {
				const StructDecl* owner = structOf(*expression.left->left);
				const auto found = owner ? _helpers.find(owner->name + "::" + expression.left->memberName) : _helpers.end();
				returned = found == _helpers.end() ? nullptr : &found->second.definition->returnType;
			}
			return returned && returned->arrayLength && !returned->isPointer ? std::optional<Type>(*returned) : std::nullopt;
		}
		default: return std::nullopt;
	}
}

const StructDecl* Emitter::indexOperatorOwner(const Expression& expression) {
	if (expression.kind != ExpressionKind::Index || expression.isDereference) return nullptr;
	if (expression.left->kind == ExpressionKind::Identifier && !expression.left->fileScope) {
		const auto it = _bindings.find(expression.left->name);
		if (it != _bindings.end() && it->second.isBuffer) return nullptr;
	}
	return arrayExpressionType(*expression.left) ? nullptr : structOf(*expression.left);
}

Emitter::HelperFunction* Emitter::indexOperatorHelper(const StructDecl& owner) {
	const auto found = _helpers.find(owner.name + "::operator[]");
	return found == _helpers.end() ? nullptr : &found->second;
}

bool Emitter::validMemberReceiver(const FunctionDecl& definition, const Expression& receiver) {
	if (!isComputedValue(receiver)) {
		const Expression& root = storeRoot(receiver);
		if (root.kind == ExpressionKind::Comma) return validMemberReceiver(definition, *root.right);
		if (root.kind == ExpressionKind::Conditional)
			return validMemberReceiver(definition, *root.arguments[0]) && validMemberReceiver(definition, *root.arguments[1]);
	}
	const AddressSpace space = definition.parameters[0].type.addressSpace;
	if (space == AddressSpace::Constant) return inConstantSpace(receiver);
	if (space != AddressSpace::Thread) return true;
	const OverloadArgument object = classifyArgument(receiver);
	return !(object.kind == OverloadArgument::Kind::Variable && !object.inThread);
}

Id Emitter::emitIndex(const Expression& expression, bool asAddress, bool allowVoid) {
	if (const StructDecl* owner = indexOperatorOwner(expression)) {
		if (asAddress) throw CompileError("expression is not assignable");
		HelperFunction* op = indexOperatorHelper(*owner);
		if (!op) throw CompileError("struct \"" + owner->name + "\" has no operator[]");
		if (op->definition == _helper)
			throw CompileError("recursive call: \"" + op->definition->name
				+ "\" calls itself; SPIR-V for Vulkan forbids recursion (Apple's compiler accepts it)");
		if (expression.arguments.size() != 1)
			throw CompileError("expected exactly one index, found " + std::to_string(expression.arguments.size()));
		if (!allowVoid && returnsVoid(*op->definition))
			throw CompileError("a call to \"" + op->definition->name + "\" returns void and has no value");
		const Id savedObject = std::exchange(_pendingObject, InvalidId);
		const Id result = emitHelperCall(expression, *op, expression.left.get(), {});
		_pendingObject = savedObject;
		return result;
	}

	size_t depth = 0;
	if (arrayRootBinding(expression, depth)) {
		return emitLocalArrayIndex(expression, asAddress);
	}

	if (const auto element = emitArrayMemberElement(expression, asAddress)) {
		return *element;
	}

	const std::optional<Binding> member = pointerMemberBinding(*expression.left);
	if (!member && isElementAccess(expression)) {
		return emitElementAccess(expression, asAddress);
	}

	if (!member && expression.left->kind != ExpressionKind::Identifier) {
		throw CompileError(expression.isDereference ? kDereferenceOperand
			: "indexing an expression is not supported yet; index a parameter or local directly");
	}

	const std::string& name = member ? expression.left->memberName : expression.left->name;
	const Binding* found = member ? &*member : findBinding(*expression.left);
	if (!found || !found->isPointer) {
		throw CompileError("\"" + name + "\" is not a pointer, so it cannot "
			"be indexed");
	}

	const Binding& binding = *found;

	if (expression.isDereference && !binding.isBuffer && !binding.pointeeMsl.arrayLength) {
		throw CompileError(notAPointer(name));
	}
	if (binding.pointeeMsl.isAtomic) {
		throw CompileError("\"" + name + "\" points at atomics, which are read and written "
			"only by the atomic_*_explicit functions");
	}

	if (expression.arguments.size() != 1) {
		throw CompileError("expected exactly one index, found " +
			std::to_string(expression.arguments.size()));
	}

	const Id index = stepIndex(*expression.arguments[0],
		binding.bufferPointeeType != InvalidId ? "buffer element" : "matrix column");

	// A buffer is reached through its address rather than a descriptor, so
	// the access chain starts from the loaded pointer. The result is a
	// pointer in the buffer's own storage class, because the result class of
	// an access chain has to match its base.
	if (binding.bufferPointeeType != InvalidId) {
		const Id base = bufferBase(binding);

		// A matrix reference holds the address of the matrix itself, so an index
		// names one of its columns.
		if (!binding.isBuffer) {
			if (const auto* matrix = _types.matrixInfo(binding.pointeeType)) {
				const Id column = _builder.emitTyped(spirv::OpAccessChain,
					_types.pointer(spirv::StorageClass::PhysicalStorageBuffer, matrix->column),
					{ base, index });
				return asAddress ? column : loadFromBuffer(column, matrix->column);
			}
		}

		const bool packed = binding.pointeeMsl.isPacked;
		const bool bytes = _types.isBool(binding.pointeeType);
		const Id resultType = _types.pointer(spirv::StorageClass::PhysicalStorageBuffer,
			packed ? _types.packedStorage(binding.pointeeMsl.scalar, binding.pointeeMsl.vectorWidth)
				: bytes ? _types.boolStorage(binding.pointeeType)
				: binding.pointeeType);

		// A buffer's pointee is { T runtime_array[] }, so an element is member
		// 0 and then the index. A struct pointee is indexed directly.
		Id address = binding.isBuffer
			? _builder.emitTyped(spirv::OpAccessChain, resultType,
				{ base, constantU32(0), index })
			: _builder.emitTyped(spirv::OpAccessChain, resultType, { base, index });

		if (packed) {
			address = packedVectorAddress(address, binding.pointeeType);
		} else if (bytes) {
			address = boolAddress(address, binding.pointeeType);
		}

		return asAddress ? address : loadFromBuffer(address, binding.pointeeType);
	}

	// Indexing a matrix names one of its columns, not another matrix, and an array
	// names one of its elements.
	const auto* matrix = _types.matrixInfo(binding.pointeeType);
	const Id arrayElement = _types.arrayElement(binding.pointeeType);
	if (arrayElement != InvalidId) {
		const Expression& at = *expression.arguments[0];
		if (const auto literal = literalIndex(at); literal && *literal >= *binding.pointeeMsl.arrayLength) {
			throw CompileError("the index " + std::to_string(*literal) + " is outside the array \""
				+ expression.left->name + "\" of " + std::to_string(*binding.pointeeMsl.arrayLength));
		}
	}
	const Id elementType = arrayElement != InvalidId ? arrayElement : matrix ? matrix->column : binding.pointeeType;
	const Id address = _builder.emitTyped(spirv::OpAccessChain,
		_types.pointer(binding.storageClass, elementType),
		{ binding.id, index });

	// A reference local bound to a place in a buffer holds that place's address,
	// so a load through it carries the buffer's Aligned operand.
	if (binding.storageClass == spirv::StorageClass::PhysicalStorageBuffer) {
		return asAddress ? address : loadFromBuffer(address, elementType);
	}

	return asAddress ? address : loadFrom(address, elementType);
}

// An index whose left side is a vector, as opposed to a buffer or a matrix. A
// buffer pointer and a matrix are the names whose index is one access
// chain step; anything else that is indexed is a value, and the element access
// finds out whether it is a vector.
bool Emitter::isElementAccess(const Expression& expression) const {
	if (expression.isDereference) {
		return false;
	}

	if (expression.left->kind != ExpressionKind::Identifier) {
		return true;
	}

	const Binding* found = findBinding(*expression.left);
	if (!found) {
		return _constants.count(expression.left->name) > 0;
	}
	return !found->isBuffer && !_types.matrixInfo(found->pointeeType)
		&& _types.arrayElement(found->pointeeType) == InvalidId;
}

// The index of a literal, with a sign. Anything else is not known here and is
// emitted and indexed at run time.
std::optional<int64_t> literalIndex(const Expression& expression) {
	constexpr uint64_t kBeyondAnyWidth = uint64_t(1) << 40;
	switch (expression.kind) {
		case ExpressionKind::BoolLiteral: return expression.boolValue ? 1 : 0;
		case ExpressionKind::IntLiteral:
			if (expression.enumName == "metal::component") {
				return std::nullopt;
			}
			return static_cast<int64_t>(std::min(expression.intValue, kBeyondAnyWidth));
		case ExpressionKind::Unary: {
			const auto operand = literalIndex(*expression.left);
			if (!operand) {
				return std::nullopt;
			}
			if (expression.unaryOperator == UnaryOperator::Plus) {
				return operand;
			}
			return expression.unaryOperator == UnaryOperator::Negate
				? std::optional<int64_t>(-*operand) : std::nullopt;
		}
		default: return std::nullopt;
	}
}

// Apple takes any integer, or a bool, as the index of a vector and leaves an
// out-of-range one undefined. A literal that is out of range cannot be
// lowered to a valid module, so it is rejected; a run-time one is not checked,
// which is the same undefined behaviour. The index is made 32 bits wide here
// and its own type is checked here, so the lowering does not depend on what
// the generic index path relabels it as.
Emitter::ElementIndex Emitter::elementIndex(const Expression& expression, uint32_t width) {
	requireEnumValue("", expression, "a vector index");
	if (const auto literal = literalIndex(expression)) {
		if (*literal < 0 || *literal >= static_cast<int64_t>(width)) {
			throw CompileError("the index " + std::to_string(*literal) + " is outside a vector of "
				+ std::to_string(width) + " components");
		}
		return { static_cast<uint32_t>(*literal), InvalidId };
	}

	return { std::nullopt, wordIndex(emitExpression(expression), "vector") };
}

// An index operand as a 32-bit integer id. Apple takes any integer, or a bool,
// and nothing else: a float, a vector, a struct or a pointer is not an index.
Id Emitter::wordIndex(Id index, const char* what) {
	const Id type = _builder.typeOf(index);
	if (type == _boolType) {
		return convert(index, type, _uintType);
	}

	if (_types.vectorWidth(type) != 1 || _types.bitWidth(type) == 0 || _types.isFloat(type)) {
		throw CompileError(std::string("the index of a ") + what + " has to be an integer or a bool");
	}

	return _types.bitWidth(type) == 32 ? index : convert(index, type, _uintType);
}

Id Emitter::stepIndex(const Expression& expression, const char* what) {
	const Id index = wordIndex(emitNonComponent(expression, std::string("the ") + what + " index"), what);
	_builder.setType(index, _uintType);
	return index;
}

// v[i] where v is a vector: one component of it. As a value it is taken out of
// the loaded vector. As a place it is an access chain into the vector's own
// storage, so a store changes that lane and no other.
// m[i] where m is a matrix value, such as a constant: column i. A composite has no
// run-time extract, so a variable index goes through a copy in a Function variable.
Id Emitter::emitMatrixValueColumn(Id matrixValue, const TypeTable::MatrixInfo& matrix, const Expression& index) {
	if (const auto literal = literalIndex(index)) {
		if (*literal < 0 || *literal >= static_cast<int64_t>(matrix.columns)) {
			throw CompileError("the index " + std::to_string(*literal) + " is outside a matrix of "
				+ std::to_string(matrix.columns) + " columns");
		}
		return _builder.emitTyped(spirv::OpCompositeExtract, matrix.column,
			{ matrixValue, static_cast<uint32_t>(*literal) });
	}

	const Id at = stepIndex(index, "matrix column");
	const Id matrixType = _builder.typeOf(matrixValue);
	const Id copy = _builder.emitDeclTyped(spirv::OpVariable,
		_types.pointer(spirv::StorageClass::Function, matrixType),
		{ static_cast<uint32_t>(spirv::StorageClass::Function) });
	_builder.emit(spirv::OpStore, { copy, matrixValue });
	const Id address = _builder.emitTyped(spirv::OpAccessChain,
		_types.pointer(spirv::StorageClass::Function, matrix.column), { copy, at });
	return loadFrom(address, matrix.column);
}

Id Emitter::emitElementAccess(const Expression& expression, bool asAddress) {
	const Expression& base = *expression.left;
	const std::string notAVector = "only an element of a vector is lowered here; a matrix element "
		"of a buffer or a struct member is not lowered yet";

	if (!asAddress) {
		const Id vector = emitExpression(base);
		const Id type = _builder.typeOf(vector);
		if (const Id element = _types.arrayElement(type); element != InvalidId) {
			// An element of an array value (a call's result): a constant index is read out of the
			// value, any other one through a temporary the value is stored in.
			const Expression& at = *expression.arguments[0];
			if (const auto literal = literalIndex(at); literal && (*literal < 0 || *literal >= _types.arrayLength(type))) {
				throw CompileError("the index " + std::to_string(*literal) + " is outside an array of "
					+ std::to_string(_types.arrayLength(type)));
			} else if (literal) {
				return _builder.emitTyped(spirv::OpCompositeExtract, element, { vector, static_cast<uint32_t>(*literal) });
			}
			const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
				_types.pointer(spirv::StorageClass::Function, type),
				{ static_cast<uint32_t>(spirv::StorageClass::Function) });
			_builder.emit(spirv::OpStore, { variable, vector });
			const Id address = _builder.emitTyped(spirv::OpAccessChain,
				_types.pointer(spirv::StorageClass::Function, element), { variable, stepIndex(at, "array element") });
			return loadFrom(address, element);
		}
		if (const auto* matrix = _types.matrixInfo(type)) {
			return emitMatrixValueColumn(vector, *matrix, *expression.arguments[0]);
		}
		const Id component = _types.componentOf(type);
		if (component == InvalidId) {
			throw CompileError(notAVector);
		}

		const ElementIndex index = elementIndex(*expression.arguments[0], _types.vectorWidth(type));
		return index.constant
			? _builder.emitTyped(spirv::OpCompositeExtract, component, { vector, *index.constant })
			: _builder.emitTyped(spirv::OpVectorExtractDynamic, component, { vector, index.id });
	}

	if (base.kind == ExpressionKind::Member && !structOf(*base.left)) {
		throw CompileError("assigning to an element of a swizzle is not lowered yet");
	}

	const Id address = emitPlaceAddress(base);
	const Id vectorType = valueTypeAt(address);
	const auto storageClass = _types.storageClassOf(_builder.typeOf(address));
	if (vectorType == InvalidId || !storageClass) {
		throw CompileError("assigning to an element of a value that is not a local, a struct "
			"member or a buffer element");
	}

	const Id component = _types.componentOf(vectorType);
	if (component == InvalidId) {
		throw CompileError(notAVector);
	}

	const ElementIndex index = elementIndex(*expression.arguments[0], _types.vectorWidth(vectorType));

	// The lanes of a bool vector in a buffer are bytes of an array, so the lane is
	// the byte's address, and a store through it still converts to a bool first.
	if (_boolAddresses.count(address) > 0) {
		return boolAddress(_builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(*storageClass, _types.scalar(ScalarKind::UChar)),
			{ address, index.constant ? constantU32(*index.constant) : index.id }), component);
	}

	return _builder.emitTyped(spirv::OpAccessChain, _types.pointer(*storageClass, component),
		{ address, index.constant ? constantU32(*index.constant) : index.id });
}

} // namespace mslc
