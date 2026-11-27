#include "sema/emitter.h"

namespace mslc {

Id Emitter::constantU32(uint32_t value) {
	return _builder.emitDeclTyped(spirv::OpConstant, _uintType, { value });
}

Id Emitter::declaredTypeOf(const Type& type) {
	// A pointer and an array are not one value each, and answering with the
	// base type says something narrower than the type that was asked about. A
	// *local* of one of them is then declared as a base, and its initialiser is
	// converted to that base and stored, so "float[2] v;" became one float and
	// "float *p = 0;" wrote a float 0.0 into a pointer. Both validate, and both
	// read as nonsense afterwards.
	//
	// The check is in the local path and not here, because a buffer parameter
	// is a pointer by definition: that is how every parameter reaches the
	// binding-0 block, and the base type is exactly what a member of it has to
	// point at. This function answers the value a type denotes, which for a
	// pointer is its pointee, and refusing it here would refuse every buffer.
	// An array<T, N> is a value of its array type; a T[N] has only its elements as values.
	if (type.isStdArray && type.arrayLength && !type.isPointer) {
		return arrayTypeOf(type);
	}

	if (!type.namedType.empty()) {
		const Id id = _types.valueStruct(type.namedType);
		if (id == InvalidId) {
			throw CompileError("undeclared type \"" + type.namedType + "\"");
		}

		return id;
	}

	if (type.isMatrix()) {
		return _types.matrix(type.scalar, type.matrixColumns, type.vectorWidth);
	}

	if (type.vectorWidth > 1) {
		return _types.vector(type.scalar, type.vectorWidth);
	}

	return _types.scalar(type.scalar);
}


// The type an integer operand is promoted to: int, keeping the operand's shape.
// The caller has already established that the operand is narrower than int, so
// the only thing left to know is whether it is a vector.
Id Emitter::promotedTo(uint32_t components) const {
	return components > 1
		? _types.vector(ScalarKind::Int, components)
		: _types.scalar(ScalarKind::Int);
}

Id Emitter::loadFrom(Id pointer, Id pointeeType) {
	return _builder.emitTyped(spirv::OpLoad, pointeeType, { pointer });
}

// A load or store through a PhysicalStorageBuffer pointer must carry the
// Aligned memory operand; spirv-val rejects the module without it. There is
// no exemption for an aggregate: the VUID is about the access, not the type
// it accesses, and a float3 element is the first aggregate load mslc emits.
std::vector<uint32_t> Emitter::alignedOperands(Id pointer, Id pointeeType) const {
	return { static_cast<uint32_t>(spirv::MemoryAccess::Aligned),
		_types.alignmentOf(pointeeType, _packedAddresses.count(pointer) > 0) };
}

Id Emitter::packedVectorAddress(Id storageAddress, Id vectorType) {
	const Id address = _builder.emitTyped(spirv::OpBitcast,
		_types.pointer(spirv::StorageClass::PhysicalStorageBuffer, vectorType), { storageAddress });
	_packedAddresses.insert(address);
	return address;
}

Id Emitter::boolAddress(Id storageAddress, Id boolType) {
	_boolAddresses.emplace(storageAddress, boolType);
	return storageAddress;
}

Id Emitter::valueTypeAt(Id address) const {
	const auto bytes = _boolAddresses.find(address);
	return bytes != _boolAddresses.end() ? bytes->second
		: _types.pointeeOf(_builder.typeOf(address));
}

// A byte, or an array of bytes, as the bool or bool vector it holds: a lane is
// true when its byte is not zero. Apple leaves a byte other than 0 or 1
// undefined; a nonzero byte is true here.
Id Emitter::boolFromStorage(Id bytes, Id boolType) {
	const uint32_t width = _types.vectorWidth(boolType);
	if (width == 1) {
		return convert(bytes, _builder.typeOf(bytes), boolType);
	}

	const Id byteType = _types.scalar(ScalarKind::UChar);
	const Id laneType = _types.scalar(ScalarKind::Bool);
	std::vector<uint32_t> lanes;
	for (uint32_t lane = 0; lane < width; ++lane) {
		const Id byte = _builder.emitTyped(spirv::OpCompositeExtract, byteType, { bytes, lane });
		lanes.push_back(convert(byte, byteType, laneType));
	}

	return _builder.emitTyped(spirv::OpCompositeConstruct, boolType, lanes);
}

// A bool or bool vector as the byte or bytes a buffer holds: 1 for true, 0
// for false, never the bool's own bits.
Id Emitter::boolToStorage(Id value) {
	const Id boolType = _builder.typeOf(value);
	const uint32_t width = _types.vectorWidth(boolType);
	const Id byteType = _types.scalar(ScalarKind::UChar);
	if (width == 1) {
		return convert(value, boolType, byteType);
	}

	const Id laneType = _types.scalar(ScalarKind::Bool);
	std::vector<uint32_t> lanes;
	for (uint32_t lane = 0; lane < width; ++lane) {
		const Id flag = _builder.emitTyped(spirv::OpCompositeExtract, laneType, { value, lane });
		lanes.push_back(convert(flag, laneType, byteType));
	}

	return _builder.emitTyped(spirv::OpCompositeConstruct,
		_types.boolStorage(boolType), lanes);
}

Id Emitter::loadFromBuffer(Id pointer, Id pointeeType) {
	const auto bytes = _boolAddresses.find(pointer);
	const Id loadedType = bytes == _boolAddresses.end()
		? pointeeType : _types.pointeeOf(_builder.typeOf(pointer));
	std::vector<uint32_t> operands = alignedOperands(pointer, pointeeType);
	operands.insert(operands.begin(), pointer);
	const Id loaded = _builder.emitTyped(spirv::OpLoad, loadedType, operands);
	return bytes == _boolAddresses.end() ? loaded : boolFromStorage(loaded, bytes->second);
}

void Emitter::storeIntoBuffer(Id pointer, Id value) {
	const Id pointeeType = valueTypeAt(pointer);
	if (_boolAddresses.count(pointer) > 0) {
		value = boolToStorage(value);
	}

	// Inserted one at a time: insert(pos, a, b) with two integers is the
	// count-and-value overload, which would insert a copies of b.
	std::vector<uint32_t> operands = alignedOperands(pointer, pointeeType);
	operands.insert(operands.begin(), pointer);
	operands.insert(operands.begin() + 1, value);
	_builder.emit(spirv::OpStore, operands);
}

// The buffer's own pointer, loaded from its member of the binding-0 block.
// Loaded once per parameter: the load is what turns the address indium wrote
// into a pointer the access chain can walk.
Id Emitter::bufferBase(const Binding& binding) {
	// A helper's device or constant pointer parameter holds the pointer as
	// its own parameter value; there is no address-block member to load.
	if (binding.ownAddress) {
		return binding.id;
	}
	const auto cached = _bufferBases.find(binding.memberIndex);
	if (cached == _bufferBases.end()) {
		throw CompileError("no address was loaded for this buffer, so it cannot be used");
	}

	return cached->second;
}

// The laid-out element type of a device or constant pointer's pointee, and the
// { T runtime_array[] } struct the pointer is to. A named struct element is the
// laid-out array-element form, not the value struct, exactly as an entry
// point's own buffer parameter is bound.
std::pair<Id, Id> Emitter::bufferParamTypes(const Type& type) {
	const Id pointee = type.namedType.empty()
		? declaredTypeOf(type)
		: _types.arrayElementStruct(type.namedType);
	if (pointee == InvalidId) {
		throw CompileError("undeclared type \"" + type.namedType + "\"");
	}
	return { pointee, _types.blockStructFor(pointee, type.isPacked) };
}

// A struct is declared once as a value and once as a buffer's laid-out
// element, so a copy between a local and a buffer is two types for one struct.
// OpBitcast takes no struct, so the copy is member by member: the members are
// pulled out of the source and the target is built from them, with a packed
// vector turned between the array of components a laid-out struct stores and
// the vector a local holds.
// An array of one form as the same array of the other: the elements are the
// same type, and only the array types differ, by the stride a laid-out one has.
Id Emitter::convertArray(Id value, Id fromType, Id toType) {
	const Id fromElement = _types.arrayElement(fromType);
	const Id toElement = _types.arrayElement(toType);
	std::vector<uint32_t> elements;
	for (uint32_t i = 0; i < _types.arrayLength(fromType); ++i) {
		Id element = _builder.emitTyped(spirv::OpCompositeExtract, fromElement, { value, i });
		if (fromElement != toElement) {
			element = convert(element, fromElement, toElement);
		}
		elements.push_back(element);
	}
	return _builder.emitTyped(spirv::OpCompositeConstruct, toType, elements);
}

Id Emitter::convertStruct(Id value, const TypeTable::StructForm& from,
	const TypeTable::StructForm& to, Id toType) {
	if (from.name != to.name) {
		throw CompileError("the struct " + from.name + " is used where the struct " + to.name
			+ " is expected, and mslc converts no struct to another");
	}

	std::vector<uint32_t> members;
	for (size_t i = 0; i < from.declared.size(); ++i) {
		Id member = _builder.emitTyped(spirv::OpCompositeExtract, from.declared[i],
			{ value, static_cast<uint32_t>(i) });
		TypeTable::StructForm memberForm;

		if (from.declared[i] != to.declared[i] && _types.arrayElement(from.declared[i]) != InvalidId) {
			member = convertArray(member, from.declared[i], to.declared[i]);
		} else if (from.declared[i] != to.declared[i] && _types.structForm(from.declared[i], memberForm)) {
			member = convert(member, from.declared[i], to.declared[i]);
		} else if (from.declared[i] != to.declared[i] && _types.isBool(from.value[i])) {
			// A bool member is bytes in the laid-out form and a bool in the value
			// form, whichever way the copy goes.
			member = from.declared[i] == from.value[i]
				? boolToStorage(member) : boolFromStorage(member, from.value[i]);
		} else if (from.declared[i] != to.declared[i]) {
			const Id vector = from.value[i];
			const Id component = _types.componentOf(vector);
			const uint32_t width = _types.vectorWidth(vector);
			std::vector<uint32_t> parts;
			for (uint32_t lane = 0; lane < width; ++lane) {
				parts.push_back(_builder.emitTyped(spirv::OpCompositeExtract, component,
					{ member, lane }));
			}
			member = _builder.emitTyped(spirv::OpCompositeConstruct, to.declared[i], parts);
		}

		members.push_back(member);
	}

	return composeStruct(toType, members);
}

// The constant form of composeStruct.
Id Emitter::constantStruct(Id type, const std::vector<uint32_t>& parts) {
	return parts.empty() ? _builder.emitDeclTyped(spirv::OpConstantNull, type, { })
		: _builder.emitDeclTyped(spirv::OpConstantComposite, type, parts);
}

// An empty struct has no constituents to construct from, so it is its null value.
Id Emitter::composeStruct(Id type, const std::vector<uint32_t>& parts) {
	return parts.empty() ? _builder.emitDeclTyped(spirv::OpConstantNull, type, { })
		: _builder.emitTyped(spirv::OpCompositeConstruct, type, parts);
}

Id Emitter::convert(Id value, Id fromType, Id toType) {
	if (fromType == toType) {
		return value;
	}

	// An array of one layout as the same array of another (a stride differs between a buffer's
	// and a local's); a different length or element is no conversion.
	if (_types.arrayElement(fromType) != InvalidId || _types.arrayElement(toType) != InvalidId) {
		if (_types.arrayElement(fromType) == InvalidId || _types.arrayElement(toType) == InvalidId
			|| _types.arrayLength(fromType) != _types.arrayLength(toType)) {
			throw CompileError("an array is used where an array of another length or element type is expected");
		}
		return convertArray(value, fromType, toType);
	}

	TypeTable::StructForm fromForm;
	TypeTable::StructForm toForm;
	if (_types.structForm(fromType, fromForm) && _types.structForm(toType, toForm)) {
		return convertStruct(value, fromForm, toForm, toType);
	}

	// A struct converts to another form of itself and to nothing else. A bool
	// keeps its own diagnostic below.
	if ((_types.structForm(fromType, fromForm) && !_types.isBool(toType))
		|| (_types.structForm(toType, toForm) && !_types.isBool(fromType))) {
		throw CompileError("a struct converts to another form of itself only");
	}

	// Every convert opcode takes a scalar or a vector, so a matrix of another
	// shape or component type, or a matrix where a scalar or vector belongs, has
	// no instruction to lower to.
	if (_types.matrixInfo(fromType) || _types.matrixInfo(toType)) {
		throw CompileError("a matrix is used where a value of another type is expected, "
			"and mslc converts no matrix to or from another type");
	}

	// A scalar is not a value of a vector type, and neither vector is a value
	// of the other's width, so there is no conversion between them:
	// OpFConvert and OpBitcast both reject a change of component count, and
	// OpCompositeConstruct would be a broadcast, which is what the arithmetic
	// path asks for explicitly and what an initialiser or a store does not
	// mean.
	const uint32_t fromWidth = _types.vectorWidth(fromType);
	const uint32_t toWidth = _types.vectorWidth(toType);
	if (fromWidth != toWidth && (fromWidth > 1 || toWidth > 1)) {
		throw CompileError(fromWidth == 1 || toWidth == 1
			? "a scalar cannot be converted to a vector"
			: "a vector of " + std::to_string(fromWidth) + " components cannot be "
				"converted to one of " + std::to_string(toWidth));
	}

	// No convert opcode takes a bool, scalar or vector. To a bool, C++ says
	// "not equal to zero", and a NaN is not equal to anything, so it is true:
	// the float compare is the unordered one. From a bool, true is one.
	const bool toBool = _types.isBool(toType);
	if (toBool || _types.isBool(fromType)) {
		if (_types.bitWidth(toBool ? fromType : toType) == 0) {
			throw CompileError("a bool converts to and from a numeric scalar or vector only");
		}

		return toBool
			? _builder.emitTyped(comparisonOpcode(BinaryOperator::NotEqual,
					_types.isFloat(fromType), false),
				toType, { value, _types.zero(fromType) })
			: _builder.emitTyped(spirv::OpSelect, toType,
				{ value, _types.one(toType), _types.zero(toType) });
	}

	// Integer and float conversions each have their own opcodes. OpBitcast is
	// a reinterpretation, which the spec permits only between operands of
	// equal width, so anything that changes a width is a convert instead.
	const bool fromFloat = _types.isFloat(fromType);
	const bool toFloat = _types.isFloat(toType);

	if (fromFloat && !toFloat) {
		const bool toSigned = _types.isSignedInt(toType);
		return _builder.emitTyped(toSigned ? spirv::OpConvertFToS : spirv::OpConvertFToU, toType, { value });
	}

	if (!fromFloat && toFloat) {
		// The source's signedness picks the opcode: OpConvertUToF zero-extends
		// and OpConvertSToF sign-extends, so the wrong one turns every negative
		// int into a large positive float.
		return _builder.emitTyped(_types.isSignedInt(fromType)
				? spirv::OpConvertSToF : spirv::OpConvertUToF,
			toType, { value });
	}

	if (fromFloat) {
		return _builder.emitTyped(spirv::OpFConvert, toType, { value });
	}

	// An integer to an integer. OpBitcast is a reinterpretation, which is what
	// a change of signedness at the same width is, and the spec permits it only
	// at equal width. Across widths the opcode is the extension itself:
	// OpSConvert sign extends or truncates, OpUConvert zero extends or
	// truncates, and spirv-val requires the opcode to match the result type's
	// signedness.
	const uint32_t fromBits = _types.bitWidth(fromType);
	const uint32_t toBits = _types.bitWidth(toType);
	if (fromBits == toBits) {
		return _builder.emitTyped(spirv::OpBitcast, toType, { value });
	}

	const bool fromSigned = _types.isSignedInt(fromType);
	const bool toSigned = _types.isSignedInt(toType);

	// Truncation keeps the low bits whatever the signedness, so the result's
	// own signedness picks the opcode and the value survives.
	if (toBits < fromBits) {
		return _builder.emitTyped(toSigned ? spirv::OpSConvert : spirv::OpUConvert,
			toType, { value });
	}

	if (fromSigned == toSigned) {
		return _builder.emitTyped(fromSigned ? spirv::OpSConvert : spirv::OpUConvert,
			toType, { value });
	}

	// Widening across a change of signedness. The extension has to follow the
	// value being extended, not the type it ends up in, so the value goes to
	// a temporary of the source's signedness at the target width and is then
	// reinterpreted. Extending straight to the result type would zero extend
	// a negative short into a small positive ulong, or sign extend 65535u into
	// a negative int.
	const auto temporary = integerKind(toBits, fromSigned);
	if (!temporary) {
		throw CompileError("converting between integer widths mslc does not know "
			"is not lowered yet");
	}

	const Id widened = _builder.emitTyped(
		fromSigned ? spirv::OpSConvert : spirv::OpUConvert,
		_types.vector(*temporary, fromWidth), { value });
	return _builder.emitTyped(spirv::OpBitcast, toType, { widened });
}

// The conversion an initialiser or an assignment makes without a cast. Apple
// takes a scalar there and fills every component with it, and rejects a
// vector of another component type, so int2 does not become float2 or bool2
// until it is written as one.
Id Emitter::convertImplicit(Id value, Id toType) {
	const Id fromType = _builder.typeOf(value);
	if (_types.vectorWidth(fromType) == 1 && _types.vectorWidth(toType) > 1
		&& _types.bitWidth(fromType) != 0) {
		return broadcast(value, toType);
	}

	if (fromType != toType && _types.vectorWidth(fromType) > 1 && _types.vectorWidth(toType) > 1
		&& _types.vectorWidth(fromType) == _types.vectorWidth(toType)) {
		throw CompileError("a vector is not implicitly converted to a vector of another "
			"component type; write the conversion as a constructor");
	}

	return convert(value, fromType, toType);
}

// A condition, !, && and || take a numeric scalar and compare it with zero, as
// C++ does. A vector is no condition: Apple rejects one.
Id Emitter::asCondition(Id value) {
	const Id type = _builder.typeOf(value);
	if (type == _boolType) {
		return value;
	}

	if (_types.vectorWidth(type) > 1) {
		throw CompileError("a condition has to be a bool or a numeric scalar");
	}

	return convert(value, type, _boolType);
}

Id Emitter::emitCondition(const Expression& expression) {
	return asCondition(emitNonComponent(expression, "a condition"));
}

Id Emitter::emitIdentifier(const Expression& expression) {
	// The entry point's own names first, then the module's: a constant declared
	// at file scope is not a parameter of the entry point that reads it.
	const Binding* found = findResourceBinding(expression.name, expression.fileScope);
	if (!found) {
		if (_functionConstants.count(expression.name)) {
			return functionConstantValue(expression.name, false);
		}
		const auto alias = _functionConstantDefinedAliases.find(expression.name);
		if (alias != _functionConstantDefinedAliases.end()) {
			return functionConstantValue(alias->second, true);
		}
		if (_derivedFunctionConstants.count(expression.name)) {
			return derivedFunctionConstantValue(expression.name);
		}
		const auto constant = _constants.find(expression.name);
		if (constant != _constants.end()) {
			return constant->second.id;
		}

		// A parameter is free to be named after an MSL builtin: Blender's
		// compute_buffer_clear names one "position". Reporting the collision
		// here rather than in the parser is what lets the declaration win.
		if (isMSLBuiltinName(expression.name)) {
			throw CompileError("builtin function \"" + expression.name
				+ "\" is not supported yet");
		}

		throw CompileError("\"" + expression.name + "\" is not a parameter, local, constant "
			"or builtin mslc knows about");
	}

	const Binding& binding = *found;
	if (binding.pointeeMsl.isAtomic) {
		throw CompileError("the atomic \"" + expression.name + "\" cannot be read as an ordinary value; "
			"use atomic_load_explicit");
	}

	if (binding.pointeeMsl.isStdArray && binding.pointeeMsl.arrayLength && binding.isPointer && !binding.isBuffer
		&& binding.bufferPointeeType == InvalidId && isLocalStorage(binding.storageClass) && !binding.isAddress) {
		return loadFrom(binding.id, binding.pointeeType);
	}

	if (binding.pointeeMsl.arrayLength && !binding.isBuffer && binding.bufferPointeeType == InvalidId) {
		throw CompileError("\"" + expression.name + "\" is an array, which is not a value: index it, as "
			+ expression.name + "[i]");
	}

	if (binding.pointeeMsl.resource != ResourceKind::None) {
		throw CompileError("\"" + expression.name + "\" is a " + typeName(binding.pointeeMsl)
			+ ", which mslc uses as the receiver of a texture call or as sample's sampler "
				"argument only");
	}

	// A buffer is reached through the address block, so the binding has no id
	// of its own to load, and loading it wrote an OpLoad of id 0.
	if (binding.bufferPointeeType != InvalidId) {
		if (binding.isBuffer) {
			throw CompileError("the buffer \"" + expression.name + "\" is used as a value (a comparison, a conversion "
				"to bool or a copy); mslc does not lower pointer values yet");
		}

		// A reference parameter names the one value its buffer holds, so its
		// use as a value reads the whole of it.
		const Id loaded = loadFromBuffer(bufferBase(binding), binding.pointeeType);
		const Id declared = declaredTypeOf(binding.pointeeMsl);
		return _builder.typeOf(loaded) == declared ? loaded : convertImplicit(loaded, declared);
	}

	if (!binding.isPointer) {
		return binding.id;
	}

	// A name bound to a pointer is an lvalue here: the expression's value
	// is what the pointer addresses, so load it. Index and member
	// expressions ask for the address itself, and go through the address
	// path instead.
	if (binding.storageClass == spirv::StorageClass::PhysicalStorageBuffer) {
		const Id inBuffer = loadFromBuffer(binding.id, binding.pointeeType);
		const Id declared = declaredTypeOf(binding.pointeeMsl);
		return _builder.typeOf(inBuffer) == declared ? inBuffer : convertImplicit(inBuffer, declared);
	}

	const Id loaded = loadFrom(binding.id, binding.pointeeType);

	// A compute id is the leading lanes of its uvec3, in the declared
	// element type: "uint index [[thread_position_in_grid]]" is
	// GlobalInvocationId.x, and a ushort2 is its .xy narrowed.
	if (binding.leadingLanes > 0) {
		const Id narrowed = binding.leadingLanes == 1
			? _builder.emitTyped(spirv::OpCompositeExtract, _types.scalar(ScalarKind::UInt), { loaded, 0u })
			: binding.leadingLanes == 2
			? _builder.emitTyped(spirv::OpVectorShuffle,
				_types.vector(ScalarKind::UInt, 2), { loaded, loaded, 0u, 1u })
			: loaded;
		return convert(narrowed, _builder.typeOf(narrowed), declaredTypeOf(binding.pointeeMsl));
	}

	return loaded;
}

} // namespace mslc
