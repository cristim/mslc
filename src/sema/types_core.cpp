#include "sema/emitter.h"

namespace mslc {

Id TypeTable::voidType() {
	if (_voidType != InvalidId) {
		return _voidType;
	}

	_voidType = _builder.emitDecl(spirv::OpTypeVoid);
	return _voidType;
}

Id TypeTable::scalar(ScalarKind kind) {
	const auto cached = _scalars.find(static_cast<uint32_t>(kind));
	if (cached != _scalars.end()) {
		return cached->second;
	}

	const ScalarMapping mapping = mappingFor(kind);
	if (!mapping.supported) {
		throw CompileError(std::string("type ") + scalarKindName(kind) + " has no SPIR-V mapping");
	}


	Id id;
	if (const auto capability = capabilityFor(mapping)) {
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(*capability) });
	}

	if (mapping.isFloat) {
		id = _builder.emitDecl(spirv::OpTypeFloat, { mapping.width });
	} else if (kind == ScalarKind::Bool) {
		id = _builder.emitDecl(spirv::OpTypeBool);
	} else {
		id = _builder.emitDecl(spirv::OpTypeInt,
			{ mapping.width, isSignedInteger(kind) ? 1u : 0u });
	}

	_scalars.emplace(static_cast<uint32_t>(kind), id);
	_widthOfScalar.emplace(id, mapping.width);
	return id;
}

std::optional<ScalarKind> TypeTable::scalarKindOf(Id type) const {
	for (const auto& [kind, id]: _scalars) {
		if (id == type) {
			return static_cast<ScalarKind>(kind);
		}
	}
	return std::nullopt;
}

Id TypeTable::vector(ScalarKind kind, uint32_t width) {
	if (width < 2) {
		return scalar(kind);
	}

	const auto key = std::make_pair(static_cast<uint32_t>(kind), width);
	const auto cached = _vectors.find(key);
	if (cached != _vectors.end()) {
		return cached->second;
	}

	const Id component = scalar(kind);


	// Declared at its true width. Metal pads a float3 to a vec4 register, but
	// SPIR-V vectors may be 3 components, and Vulkan requires a builtin such as
	// GlobalInvocationId to be exactly 3 components. A buffer's element stride
	// is handled by ArrayStride, so nothing else needs the padding.
	const Id id = _builder.emitDecl(spirv::OpTypeVector, { component, width });

	_vectors.emplace(key, id);
	_widthOfVector.emplace(id, width);
	return id;
}

Id TypeTable::matrix(ScalarKind kind, uint32_t columns, uint32_t rows) {
	const auto key = std::make_tuple(static_cast<uint32_t>(kind), columns, rows);
	const auto cached = _matrices.find(key);
	if (cached != _matrices.end()) {
		return cached->second;
	}

	const Id column = vector(kind, rows);
	const Id id = _builder.emitDecl(spirv::OpTypeMatrix, { column, columns });
	_matrices.emplace(key, id);
	_matrixInfo.emplace(id, MatrixInfo { kind, columns, rows, column });
	return id;
}

// A packed vector in a laid-out struct or array is its components as a plain
// array of scalars. A vector there would have to obey the Block layout rules,
// which put a 3- or 4-component vector at an offset that does not straddle a
// 16-byte boundary: a packed_float3 at offset 12 is the common case, and a
// module with one is rejected by spirv-val unless scalarBlockLayout is assumed.
// An array of scalars has no such rule, and the access casts the element's
// address back to a pointer to the vector.
Id TypeTable::packedStorage(ScalarKind kind, uint32_t width) {
	const auto key = std::make_pair(static_cast<uint32_t>(kind), width);
	const auto cached = _packedStorage.find(key);
	if (cached != _packedStorage.end()) {
		return cached->second;
	}

	const Id length = _builder.emitDeclTyped(spirv::OpConstant, scalar(ScalarKind::UInt), { width });
	const Id id = _builder.emitDecl(spirv::OpTypeArray, { scalar(kind), length });
	_builder.emit(spirv::OpDecorate, { id, static_cast<uint32_t>(spirv::Decoration::ArrayStride),
		scalarBitWidth(kind) / 8 });

	_packedStorage.emplace(key, id);
	return id;
}

// How a bool or a bool vector is held in a buffer. OpTypeBool has no layout, so
// the buffer holds bytes: one for a scalar, and an array of one byte per lane for
// a vector, which is stored lane by lane because a bool3's fourth byte is padding
// that Apple leaves alone. A load compares each byte with zero and a store writes
// 0 or 1 (see Emitter::boolFromStorage and boolToStorage).
spirv::Id TypeTable::boolStorage(spirv::Id boolType) {
	if (!_storage8Declared) {
		_builder.emit(spirv::OpCapability,
			{ static_cast<uint32_t>(spirv::Capability::StorageBuffer8BitAccess) });
		_storage8Declared = true;
	}

	const uint32_t width = vectorWidth(boolType);
	return width > 1 ? packedStorage(ScalarKind::UChar, width) : scalar(ScalarKind::UChar);
}

// The types a struct's members have where the struct is laid out, which are the
// value types except for the packed and the bool ones.
std::vector<Id> TypeTable::storageTypesFor(const std::string& name, const std::vector<Id>& valueTypes) {
	std::vector<Id> types = valueTypes;
	const StructDecl* decl = _unit.findStruct(name);
	for (size_t i = 0; i < types.size(); ++i) {
		const Type& field = decl->fields[i].type;
		if (field.isPointer) {
			continue;
		}
		if (field.arrayLength) {
			// Only the array wrappers differ between the forms: a laid-out array
			// carries ArrayStride, and the element is the same type in both.
			const std::vector<uint32_t> strides = arrayStrides(field);
			std::vector<uint32_t> lengths { *field.arrayLength };
			lengths.insert(lengths.end(), field.innerArrayLengths.begin(), field.innerArrayLengths.end());
			Id element = types[i];
			for (size_t dimension = 0; dimension < lengths.size(); ++dimension) {
				element = arrayElement(element);
			}
			for (size_t dimension = lengths.size(); dimension-- > 0;) {
				element = laidOutArray(element, lengths[dimension], strides[dimension]);
			}
			types[i] = element;
		} else if (field.isPacked) {
			types[i] = packedStorage(field.scalar, field.vectorWidth);
		} else if (field.scalar == ScalarKind::Bool && !field.isPointer && field.namedType.empty()) {
			types[i] = boolStorage(types[i]);
		}
	}

	return types;
}

const TypeTable::MatrixInfo* TypeTable::matrixInfo(Id type) const {
	const auto it = _matrixInfo.find(type);
	return it == _matrixInfo.end() ? nullptr : &it->second;
}

void TypeTable::decorateMatrixMember(Id structure, uint32_t member, Id type) {
	// A matrix array's member carries the matrix's own ColMajor and MatrixStride.
	for (Id element = arrayElement(type); element != InvalidId; element = arrayElement(type)) {
		type = element;
	}
	const auto it = _matrixInfo.find(type);
	if (it == _matrixInfo.end()) {
		return;
	}

	const uint32_t scalarBytes = scalarBitWidth(it->second.scalar) / 8;
	_builder.emit(spirv::OpMemberDecorate, { structure, member,
		static_cast<uint32_t>(spirv::Decoration::ColMajor) });
	_builder.emit(spirv::OpMemberDecorate, { structure, member,
		static_cast<uint32_t>(spirv::Decoration::MatrixStride),
		vectorLayoutFor(scalarBytes, it->second.rows).size });
}

spirv::Id TypeTable::blockStructFor(spirv::Id elementType, bool packed) {
	// Keyed by the layout as well as the type: a packed_float3 and a float3 are
	// one SPIR-V vector, and the two buffers step 12 and 16.
	const auto cached = _blockStructs.find({ elementType, packed });
	if (cached != _blockStructs.end()) {
		return cached->second;
	}

	// The stride is the element's own size in Metal's layout, which is the
	// scalar's size times the component count with a float3 rounded up to a
	// register. Taken from the same rule the struct offsets come from, so an
	// array of a struct's member and the member itself cannot disagree.
	Id storedType = elementType;
	uint32_t stride = 4;
	if (const auto it = _widthOfScalar.find(elementType); it != _widthOfScalar.end()) {
		stride = (it->second + 7) / 8;
		if (isBool(elementType)) {
			storedType = boolStorage(elementType);
		}
	} else if (const auto it = _widthOfVector.find(elementType); it != _widthOfVector.end()) {
		// The component's own size, from the scalar the vector was built from, so
		// a vector of half strides 8 and one of ulong strides 32 rather than both
		// striding as though they were floats.
		uint32_t scalarBytes = 4;
		for (const auto& [key, id]: _vectors) {
			if (id == elementType) {
				scalarBytes = (scalarBitWidth(static_cast<ScalarKind>(key.first)) + 7) / 8;
				if (packed) {
					storedType = packedStorage(static_cast<ScalarKind>(key.first), it->second);
				} else if (static_cast<ScalarKind>(key.first) == ScalarKind::Bool) {
					storedType = boolStorage(elementType);
				}
			}
		}

		stride = (packed ? packedVectorLayoutFor(scalarBytes, it->second)
			: vectorLayoutFor(scalarBytes, it->second)).size;
	} else if (const auto it = _matrixInfo.find(elementType); it != _matrixInfo.end()) {
		stride = matrixLayoutFor(scalarBitWidth(it->second.scalar) / 8,
			it->second.columns, it->second.rows).size;
	} else if (const auto it = _structSizes.find(elementType); it != _structSizes.end()) {
		// Metal rounds a struct's size up to its own alignment, and structMembersFor
		// has already done that, so this is the size and not a guess at it.
		// An empty struct is size 0 here and 1 in Metal; a zero stride is invalid.
		stride = std::max<uint32_t>(it->second, 1);
	}

	// A runtime array has no length, which is what lets the descriptor cover a
	// Metal buffer whose size is not known when the shader is compiled.
	const Id runtimeArray = _builder.emitDecl(spirv::OpTypeRuntimeArray, { storedType });
	_builder.emit(spirv::OpDecorate, { runtimeArray,
		static_cast<uint32_t>(spirv::Decoration::ArrayStride), stride });

	// The member count is not a word in the binary: SPIR-V derives it from the
	// instruction's word count, so passing one would be read as a second member.
	const Id structure = _builder.emitDecl(spirv::OpTypeStruct, { runtimeArray });

	// Block is what makes the struct a valid descriptor payload in Vulkan.
	_builder.emit(spirv::OpDecorate, { structure, static_cast<uint32_t>(spirv::Decoration::Block) });

	// A member offset is OpMemberDecorate; OpDecorate takes no member index.
	_builder.emit(spirv::OpMemberDecorate, { structure, 0,
		static_cast<uint32_t>(spirv::Decoration::Offset), 0u });
	decorateMatrixMember(structure, 0, elementType);

	_blockStructs.emplace(std::make_pair(elementType, packed), structure);
	return structure;
}

spirv::Id TypeTable::matrixReferenceStruct(spirv::Id matrixType) {
	const auto cached = _matrixReferenceStructs.find(matrixType);
	if (cached != _matrixReferenceStructs.end()) {
		return cached->second;
	}

	const Id structure = _builder.emitDecl(spirv::OpTypeStruct, { matrixType });
	_builder.emit(spirv::OpDecorate, { structure, static_cast<uint32_t>(spirv::Decoration::Block) });
	_builder.emit(spirv::OpMemberDecorate, { structure, 0,
		static_cast<uint32_t>(spirv::Decoration::Offset), 0u });
	decorateMatrixMember(structure, 0, matrixType);

	_matrixReferenceStructs.emplace(matrixType, structure);
	return structure;
}

spirv::Id TypeTable::pointeeOf(spirv::Id pointerType) const {
	for (const auto& [key, id]: _pointers) {
		if (id == pointerType) {
			return key.second;
		}
	}

	for (const auto& [pointee, id]: _bufferPointers) {
		if (id == pointerType) {
			return pointee;
		}
	}

	return spirv::InvalidId;
}

std::optional<spirv::StorageClassValue> TypeTable::storageClassOf(Id pointerType) const {
	for (const auto& [key, id]: _pointers) {
		if (id == pointerType) {
			return key.first;
		}
	}

	for (const auto& [pointee, id]: _bufferPointers) {
		if (id == pointerType) {
			return spirv::StorageClass::PhysicalStorageBuffer;
		}
	}

	return std::nullopt;
}

bool TypeTable::isFloat(spirv::Id type) const {
	for (const auto& [kind, id]: _scalars) {
		if (id == type) {
			return isFloatKind(static_cast<ScalarKind>(kind));
		}
	}

	// A vector is float when its component is, and the component is whatever
	// scalar the vector was built from.
	for (const auto& [key, id]: _vectors) {
		if (id == type) {
			return isFloatKind(static_cast<ScalarKind>(key.first));
		}
	}

	return false;
}

bool TypeTable::isBool(spirv::Id type) const {
	const auto boolScalar = _scalars.find(static_cast<uint32_t>(ScalarKind::Bool));
	if (boolScalar != _scalars.end() && boolScalar->second == type) {
		return true;
	}

	for (const auto& [key, id]: _vectors) {
		if (id == type) {
			return static_cast<ScalarKind>(key.first) == ScalarKind::Bool;
		}
	}

	return false;
}

bool TypeTable::isSignedInt(spirv::Id type) const {
	for (const auto& [kind, id]: _scalars) {
		if (id == type) {
			return isSignedInteger(static_cast<ScalarKind>(kind));
		}
	}

	for (const auto& [key, id]: _vectors) {
		if (id == type) {
			return isSignedInteger(static_cast<ScalarKind>(key.first));
		}
	}

	return false;
}

Id TypeTable::componentOf(Id vectorType) {
	return withWidth(vectorType, 1);
}

Id TypeTable::withWidth(Id vectorType, uint32_t width) {
	for (const auto& [key, id]: _vectors) {
		if (id == vectorType) {
			const auto kind = static_cast<ScalarKind>(key.first);
			return vector(kind, width);
		}
	}

	return InvalidId;
}

uint32_t TypeTable::vectorWidth(spirv::Id type) const {
	auto it = _widthOfVector.find(type);
	if (it != _widthOfVector.end()) {
		return it->second;
	}

	return 1;
}

uint32_t TypeTable::bitWidth(spirv::Id type) const {
	auto it = _widthOfScalar.find(type);
	if (it != _widthOfScalar.end()) {
		return it->second;
	}

	// A vector is as wide as its component, repeated. Both convert paths work on
	// one component at a time, so the component's width is what matters.
	for (const auto& [key, id]: _vectors) {
		if (id == type) {
			return scalarBitWidth(static_cast<ScalarKind>(key.first));
		}
	}

	return 0;
}

const std::string* TypeTable::structNameOf(spirv::Id type, bool* laidOut) const {
	const std::string* name = nullptr;
	if (laidOut) {
		*laidOut = true;
	}
	for (const auto& [key, id]: _structs) {
		if (id == type) {
			name = &key;
		}
	}
	for (const auto& [key, id]: _elementStructs) {
		if (id == type) {
			name = &key;
		}
	}
	for (const auto& [key, id]: _valueStructs) {
		if (id == type) {
			name = &key;
			if (laidOut) {
				*laidOut = false;
			}
		}
	}
	return name;
}

bool TypeTable::structForm(spirv::Id type, StructForm& out) {
	bool laidOut = true;
	const std::string* name = structNameOf(type, &laidOut);
	if (!name) {
		return false;
	}

	std::vector<uint32_t> offsets;
	uint32_t size = 0;
	out.name = *name;
	out.value.clear();
	structMembersFor(out.name, out.value, offsets, size);
	out.declared = laidOut ? storageTypesFor(out.name, out.value) : out.value;
	return true;
}

spirv::Id TypeTable::array(spirv::Id element, uint32_t length) {
	const auto key = std::make_pair(element, length);
	const auto cached = _arrays.find(key);
	if (cached != _arrays.end()) {
		return cached->second;
	}

	const Id lengthId = _builder.emitDeclTyped(spirv::OpConstant, scalar(ScalarKind::UInt), { length });
	const Id id = _builder.emitDecl(spirv::OpTypeArray, { element, lengthId });
	_arrays.emplace(key, id);
	_arrayElement.emplace(id, element);
	_arrayLengths.emplace(id, length);
	return id;
}

spirv::Id TypeTable::arrayOfLengthId(spirv::Id element, spirv::Id lengthId) {
	const Id id = _builder.emitDecl(spirv::OpTypeArray, { element, lengthId });
	_arrayElement.emplace(id, element);
	return id;
}

// An array in a laid-out struct: the same element and length as array() and a
// stride, which SPIR-V needs on a buffer's array and forbids on a Workgroup
// one, so the value form and this are two types.
spirv::Id TypeTable::laidOutArray(spirv::Id element, uint32_t length, uint32_t stride) {
	const auto key = std::make_tuple(element, length, stride);
	const auto cached = _laidOutArrays.find(key);
	if (cached != _laidOutArrays.end()) {
		return cached->second;
	}

	const Id lengthId = _builder.emitDeclTyped(spirv::OpConstant, scalar(ScalarKind::UInt), { length });
	const Id id = _builder.emitDecl(spirv::OpTypeArray, { element, lengthId });
	_builder.emit(spirv::OpDecorate, { id, static_cast<uint32_t>(spirv::Decoration::ArrayStride), stride });
	_laidOutArrays.emplace(key, id);
	_arrayElement.emplace(id, element);
	_arrayLengths.emplace(id, length);
	return id;
}

uint32_t TypeTable::arrayLength(spirv::Id type) const {
	const auto found = _arrayLengths.find(type);
	return found == _arrayLengths.end() ? 0 : found->second;
}

spirv::Id TypeTable::arrayElement(spirv::Id type) const {
	const auto found = _arrayElement.find(type);
	return found == _arrayElement.end() ? spirv::InvalidId : found->second;
}

bool TypeTable::isAggregate(spirv::Id type) const {
	if (_widthOfVector.count(type) > 0 || _matrixInfo.count(type) > 0 || _arrayElement.count(type) > 0) {
		return true;
	}

	// A struct is declared twice over, once as a value and once as a buffer's
	// block, and the two live in different maps, so both are looked through. A
	// scalar standing in for either is the case this answers.
	for (const auto& [name, id]: _structs) {
		if (id == type) {
			return true;
		}
	}

	for (const auto& [name, id]: _valueStructs) {
		if (id == type) {
			return true;
		}
	}

	return false;
}

spirv::Id TypeTable::bufferPointer(Id pointee) {
	const auto cached = _bufferPointers.find(pointee);
	if (cached != _bufferPointers.end()) {
		return cached->second;
	}

	const Id id = _builder.emitDecl(spirv::OpTypePointer,
		{ static_cast<uint32_t>(spirv::StorageClass::PhysicalStorageBuffer), pointee });
	_bufferPointers.emplace(pointee, id);
	return id;
}

// Returns the binding-0 address block for one descriptor set, building it from
// pointeeTypes and decorating its variable with descriptorSet the first time
// that set is asked for.
TypeTable::AddressBlock TypeTable::addressBlock(const std::vector<Id>& pointeeTypes,
	uint32_t descriptorSet) {
	// One block per set and member list: every buffer parameter of an entry
	// point is a member of the same one, which is what occupies binding 0, and a
	// module with both a vertex and a fragment entry point needs one each because
	// indium splits the sets by stage.
	//
	// Entry points that declare the same buffers share a block. Ones that declare
	// different buffers get a block of their own at the same set and binding:
	// indium creates a pipeline from one entry point, and Vulkan only requires
	// (set, binding) to be unique among the variables that entry point uses, so
	// the other entry point's variable is never part of that pipeline.
	const auto key = std::make_pair(descriptorSet, pointeeTypes);
	const auto cached = _addressBlocks.find(key);
	if (cached != _addressBlocks.end()) {
		return cached->second;
	}

	AddressBlock block;
	block.pointeeTypes = pointeeTypes;
	std::vector<uint32_t> memberPointers;
	memberPointers.reserve(pointeeTypes.size());

	for (Id pointee: pointeeTypes) {
		memberPointers.push_back(bufferPointer(pointee));
	}

	block.blockType = _builder.emitDecl(spirv::OpTypeStruct, memberPointers);
	_builder.emit(spirv::OpDecorate, { block.blockType,
		static_cast<uint32_t>(spirv::Decoration::Block) });

	// Member k is the k-th buffer parameter in declaration order, at byte
	// offset 8 * k. An 8-byte address is what the struct is laid out for, so
	// this is the size of the value and not a stride.
	for (size_t k = 0; k < memberPointers.size(); ++k) {
		_builder.emit(spirv::OpMemberDecorate, { block.blockType,
			static_cast<uint32_t>(k), static_cast<uint32_t>(spirv::Decoration::Offset),
			static_cast<uint32_t>(8 * k) });
	}

	block.memberPointer = _builder.emitDecl(spirv::OpTypePointer,
		{ static_cast<uint32_t>(spirv::StorageClass::Uniform), block.blockType });

	block.variable = _builder.emitDeclTyped(spirv::OpVariable, block.memberPointer,
		{ static_cast<uint32_t>(spirv::StorageClass::Uniform) });
	// The set is chosen by stage: indium builds set 0 from the vertex function
	// and set 1 from the fragment function, so a fragment shader's block has
	// to be in set 1 or indium never binds it.
	_builder.emit(spirv::OpDecorate, { block.variable,
		static_cast<uint32_t>(spirv::Decoration::DescriptorSet), descriptorSet });
	_builder.emit(spirv::OpDecorate, { block.variable,
		static_cast<uint32_t>(spirv::Decoration::Binding), 0u });

	_addressBlocks.emplace(key, block);
	return block;
}

} // namespace mslc
