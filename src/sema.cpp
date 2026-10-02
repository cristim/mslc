#include "sema.h"

#include "lexer.h"
#include "parser.h"

#include <algorithm>
#include <cstring>

namespace mslc {

namespace {

	using spirv::Id;
	using spirv::InvalidId;

	// SPIR-V needs the signedness of an integer recorded, and MSL spells signed
	// and unsigned types separately, so the mapping is explicit.
	struct ScalarMapping {
		bool supported;
		bool isFloat;
		uint32_t width;
	};

	ScalarMapping mappingFor(ScalarKind kind) {
		switch (kind) {
			case ScalarKind::Bool: return { true, false, 1 };
			case ScalarKind::Char:
			case ScalarKind::UChar: return { true, false, 8 };
			case ScalarKind::Short:
			case ScalarKind::UShort: return { true, false, 16 };
			case ScalarKind::Int:
			case ScalarKind::UInt: return { true, false, 32 };
			case ScalarKind::Long:
			case ScalarKind::ULong: return { true, false, 64 };
			case ScalarKind::Half: return { true, true, 16 };
			case ScalarKind::Float: return { true, true, 32 };
			case ScalarKind::Double: return { true, true, 64 };
			case ScalarKind::Void: return { false, false, 0 };
		}

		return { false, false, 0 };
	}

	// Where a member sits in a Metal struct, and how far an array of the struct
	// steps. Metal puts a float3 at the next multiple of 16 even though it holds
	// 12, and steps 16 too, so a float after it starts at 16 rather than 12.
	// indium's own lighting test relies on that: the host side declares
	// normalMatrix as "float normalMatrix[3][4]", a 176-byte struct whose matrix
	// has 16-byte columns.
	struct VectorLayout {
		uint32_t size;
		uint32_t alignment;
	};

	VectorLayout vectorLayoutFor(uint32_t scalarBytes, uint32_t components) {
		return { (components == 3 ? 4u : components) * scalarBytes,
			(components >= 3 ? 4u : components) * scalarBytes };
	}

	// The next multiple of the alignment at or after the offset, which is where
	// a member of that alignment starts in a struct.
	uint32_t alignTo(uint32_t offset, uint32_t alignment) {
		return (offset + alignment - 1) / alignment * alignment;
	}

	bool isSignedInteger(ScalarKind kind) {
		return kind == ScalarKind::Char || kind == ScalarKind::Short
			|| kind == ScalarKind::Int || kind == ScalarKind::Long;
	}

	// The capability a scalar type's width needs, or none when Shader already
	// covers it. A 64-bit float needs Float64, not the Int64 that covers a
	// 64-bit integer, and 8- and 16-bit integers need capabilities of their own.
	// The scalar kind for an integer of the given bit width and signedness, so a
	// conversion can name a temporary type rather than only the result type.
	std::optional<ScalarKind> integerKind(uint32_t bits, bool isSigned) {
		switch (bits) {
			case 8: return isSigned ? ScalarKind::Char : ScalarKind::UChar;
			case 16: return isSigned ? ScalarKind::Short : ScalarKind::UShort;
			case 32: return isSigned ? ScalarKind::Int : ScalarKind::UInt;
			case 64: return isSigned ? ScalarKind::Long : ScalarKind::ULong;
			default: return std::nullopt;
		}
	}

	std::optional<spirv::CapabilityValue> capabilityFor(ScalarMapping mapping) {
		if (mapping.isFloat) {
			if (mapping.width == 16) {
				return spirv::Capability::Float16;
			}
			if (mapping.width == 64) {
				return spirv::Capability::Float64;
			}

			return std::nullopt;
		}

		switch (mapping.width) {
			case 8: return spirv::Capability::Int8;
			case 16: return spirv::Capability::Int16;
			case 64: return spirv::Capability::Int64;
			default: return std::nullopt;
		}
	}

	bool isFloatKind(ScalarKind kind) {
		return mappingFor(kind).isFloat;
	}

	// Whether an operator produces a bool rather than its operand's type.
	bool isComparisonOperator(BinaryOperator op) {
		switch (op) {
			case BinaryOperator::Equal:
			case BinaryOperator::NotEqual:
			case BinaryOperator::Less:
			case BinaryOperator::LessEqual:
			case BinaryOperator::Greater:
			case BinaryOperator::GreaterEqual:
				return true;
			default:
				return false;
		}
	}

	// The SPIR-V opcode for a binary operation, given the operand type's kind.
	// SPIR-V has separate opcodes for float, signed and unsigned operands, so
	// the choice has to be made from the resolved operand type rather than
	// from the AST, which does not record it.
	uint16_t arithmeticOpcode(BinaryOperator op, bool isFloat, bool isSigned) {
		using Op = uint16_t;
#define PICK(f, s, u) ((isFloat) ? Op(f) : (isSigned) ? Op(s) : Op(u))

		switch (op) {
			case BinaryOperator::Add: return PICK(spirv::OpFAdd, spirv::OpIAdd, spirv::OpIAdd);
			case BinaryOperator::Subtract: return PICK(spirv::OpFSub, spirv::OpISub, spirv::OpISub);
			case BinaryOperator::Multiply: return PICK(spirv::OpFMul, spirv::OpIMul, spirv::OpIMul);
			case BinaryOperator::Divide: return PICK(spirv::OpFDiv, spirv::OpSDiv, spirv::OpUDiv);
			case BinaryOperator::Modulo:
				// C++ has no % on floating point (MSL spells that fmod), and a
				// signed % truncates, which is OpSRem; OpSMod floors.
				if (isFloat) {
					throw CompileError("operator % needs integer operands; use fmod for floating point");
				}
				return isSigned ? spirv::OpSRem : spirv::OpUMod;
			case BinaryOperator::BitAnd: return spirv::OpBitwiseAnd;
			case BinaryOperator::BitOr: return spirv::OpBitwiseOr;
			case BinaryOperator::BitXor: return spirv::OpBitwiseXor;
			case BinaryOperator::ShiftLeft: return spirv::OpShiftLeftLogical;
			case BinaryOperator::ShiftRight:
				return isSigned ? spirv::OpShiftRightArithmetic : spirv::OpShiftRightLogical;
			default: break;
		}

#undef PICK

		throw CompileError("this binary operator is recognised but not lowered yet");
	}

	// GLSL.std.450 instruction numbers, from the extended instruction set's
	// grammar (FMin 37, UMin 38, SMin 39, FMax 40, UMax 41, SMax 42).
	uint32_t minMaxInstruction(bool isMax, bool isFloat, bool isSigned) {
		if (isFloat) {
			return isMax ? 40u : 37u;
		}
		if (isSigned) {
			return isMax ? 42u : 39u;
		}
		return isMax ? 41u : 38u;
	}

	// The merge and function control masks. Every one of them is None: Pure
	// would promise no memory writes, which an entry point that stores to a
	// buffer breaks, and neither flattening nor unrolling changes what the
	// shader computes. Named from the generated table rather than written as 0.
	constexpr uint32_t kFunctionControlNone = spirv::FunctionControl::None;
	constexpr uint32_t kSelectionControlNone = spirv::SelectionControl::None;
	constexpr uint32_t kLoopControlNone = spirv::LoopControl::None;

	uint16_t comparisonOpcode(BinaryOperator op, bool isFloat, bool isSigned) {
		using Op = uint16_t;

		if (isFloat) {
			switch (op) {
				case BinaryOperator::Equal: return spirv::OpFOrdEqual;
				case BinaryOperator::NotEqual: return spirv::OpFOrdNotEqual;
				case BinaryOperator::Less: return spirv::OpFOrdLessThan;
				case BinaryOperator::LessEqual: return spirv::OpFOrdLessThanEqual;
				case BinaryOperator::Greater: return spirv::OpFOrdGreaterThan;
				case BinaryOperator::GreaterEqual: return spirv::OpFOrdGreaterThanEqual;
				default: break;
			}
		} else if (isSigned) {
			switch (op) {
				case BinaryOperator::Equal: return spirv::OpIEqual;
				case BinaryOperator::NotEqual: return spirv::OpINotEqual;
				case BinaryOperator::Less: return spirv::OpSLessThan;
				case BinaryOperator::LessEqual: return spirv::OpSLessThanEqual;
				case BinaryOperator::Greater: return spirv::OpSGreaterThan;
				case BinaryOperator::GreaterEqual: return spirv::OpSGreaterThanEqual;
				default: break;
			}
		} else {
			switch (op) {
				case BinaryOperator::Equal: return spirv::OpIEqual;
				case BinaryOperator::NotEqual: return spirv::OpINotEqual;
				case BinaryOperator::Less: return spirv::OpULessThan;
				case BinaryOperator::LessEqual: return spirv::OpULessThanEqual;
				case BinaryOperator::Greater: return spirv::OpUGreaterThan;
				case BinaryOperator::GreaterEqual: return spirv::OpUGreaterThanEqual;
				default: break;
			}
		}

		throw CompileError("this comparison is recognised but not lowered yet");
	}

}

//
// TypeTable
//

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

spirv::Id TypeTable::blockStructFor(spirv::Id elementType) {
	const auto cached = _blockStructs.find(elementType);
	if (cached != _blockStructs.end()) {
		return cached->second;
	}

	// A runtime array has no length, which is what lets the descriptor cover a
	// Metal buffer whose size is not known when the shader is compiled.
	const Id runtimeArray = _builder.emitDecl(spirv::OpTypeRuntimeArray, { elementType });

	// The stride is the element's own size in Metal's layout, which is the
	// scalar's size times the component count with a float3 rounded up to a
	// register. Taken from the same rule the struct offsets come from, so an
	// array of a struct's member and the member itself cannot disagree.
	uint32_t stride = 4;
	if (const auto it = _widthOfScalar.find(elementType); it != _widthOfScalar.end()) {
		stride = (it->second + 7) / 8;
	} else if (const auto it = _widthOfVector.find(elementType); it != _widthOfVector.end()) {
		// The component's own size, from the scalar the vector was built from, so
		// a vector of half strides 8 and one of ulong strides 32 rather than both
		// striding as though they were floats.
		uint32_t scalarBytes = 4;
		for (const auto& [key, id]: _vectors) {
			if (id == elementType) {
				scalarBytes = (scalarBitWidth(static_cast<ScalarKind>(key.first)) + 7) / 8;
			}
		}

		stride = vectorLayoutFor(scalarBytes, it->second).size;
	} else if (const auto it = _structSizes.find(elementType); it != _structSizes.end()) {
		// Metal rounds a struct's size up to its own alignment, and structMembersFor
		// has already done that, so this is the size and not a guess at it.
		stride = it->second;
	}

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

	_blockStructs.emplace(elementType, structure);
	return structure;
}

spirv::Id TypeTable::pointeeOf(spirv::Id pointerType) const {
	for (const auto& [key, id]: _pointers) {
		if (id == pointerType) {
			return key.second;
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
	for (const auto& [key, id]: _vectors) {
		if (id == vectorType) {
			return scalar(static_cast<ScalarKind>(key.first));
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

	uint32_t TypeTable::bitWidth(spirv::Id type) const {	auto it = _widthOfScalar.find(type);
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

bool TypeTable::isAggregate(spirv::Id type) const {
	if (_widthOfVector.count(type) > 0) {
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
	// One block per set: every buffer parameter of an entry point is a member of
	// the same one, which is what occupies binding 0, and a module with both a
	// vertex and a fragment entry point needs one each because indium splits the
	// sets by stage. A single cache would hand the fragment function the vertex
	// function's set, and its buffers would never bind.
	//
	// Two entry points in the *same* stage share a set and therefore have to
	// share the block, which only works if they declare the same buffers: the
	// block's members are typed and its count is fixed when it is declared, and
	// a second entry point indexing it with a different count or a different
	// pointee type produces an access chain whose result type does not match the
	// type the member holds. Both are hard errors rather than a silent reuse,
	// because the module that comes out is a rejected one at best.
	const auto cached = _addressBlocks.find(descriptorSet);
	if (cached != _addressBlocks.end()) {
		if (cached->second.pointeeTypes != pointeeTypes) {
			throw CompileError("two entry points sharing descriptor set "
				+ std::to_string(descriptorSet) + " bind different buffers, and one "
					"address block at binding 0 holds them; they have to agree in both "
					"number and type");
		}

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

	_addressBlocks.emplace(descriptorSet, block);
	return block;
}

uint32_t TypeTable::alignmentOf(Id type) const {
	// A struct's own alignment is not tracked, and 4 is the weaker claim, which
	// is the safe direction: a claim below what the layout gives is always sound.
	const uint32_t bits = bitWidth(type);
	if (bits == 0) {
		return 4;
	}

	// bitWidth is the component's width, so the element's own size comes from the
	// component's size and the component count. Rounded up, so a 24-bit
	// component claims 4, and taken from Metal's own rule rather than from
	// (bits * count) / 8, because a float3 is 16 bytes and 16-aligned where its
	// three floats are 12. Aligned has to name a power of two, which the Metal
	// rule gives and a size does not.
	const auto it = _widthOfVector.find(type);
	const uint32_t count = it != _widthOfVector.end() ? it->second : 1;
	const uint32_t scalarBytes = (bits + 7) / 8;
	return std::max<uint32_t>(1, vectorLayoutFor(scalarBytes, count).alignment);
}

Id TypeTable::zero(Id type) {
	// A vector or a struct has no single zero value: OpConstant is a scalar form,
	// so an aggregate needs OpConstantComposite over its parts. No local of that
	// shape parses yet, and reporting it beats emitting a scalar for it.
	if (isAggregate(type)) {
		throw CompileError("a local of an aggregate type cannot be zero initialised yet; "
			"give it an initialiser");
	}

	// A bool has no literal form, so it needs its own opcode rather than a zero
	// word, which the grammar does not accept for it.
	if (type == scalar(ScalarKind::Bool)) {
		return _builder.emitDeclTyped(spirv::OpConstantFalse, type, { });
	}

	// OpConstant's literal count is context dependent, so it follows the width:
	// one word up to 32 bits, two for 64. Writing one word for a 64-bit value is
	// a short instruction, which is what this used to do.
	return _builder.emitDeclTyped(spirv::OpConstant, type,
		std::vector<uint32_t>((bitWidth(type) + 31) / 32, 0u));
}

Id TypeTable::pointer(spirv::StorageClassValue storageClass, Id pointee) {
	const auto key = std::make_pair(storageClass, pointee);
	const auto cached = _pointers.find(key);
	if (cached != _pointers.end()) {
		return cached->second;
	}

	const Id id = _builder.emitDecl(spirv::OpTypePointer,
		{ static_cast<uint32_t>(storageClass), pointee });

	_pointers.emplace(key, id);
	return id;
}

// The member types of a struct and, where the caller wants them, where each one
// starts in a buffer. Metal lays a struct out with every member at the next
// multiple of its own alignment, so a float3 member both starts and steps 16
// bytes even though it holds 12, which is what a float member after it has to
// account for.
//
// False when no struct of that name is declared, so a caller can say so rather
// than guess.
bool TypeTable::structMembersFor(const std::string& name, std::vector<Id>& outTypes,
	std::vector<uint32_t>& outOffsets, uint32_t& outSize) {

	const StructDecl* decl = _unit.findStruct(name);
	if (!decl) {
		return false;
	}

	uint32_t offset = 0;
	uint32_t alignment = 1;

	for (const StructField& field: decl->fields) {
		if (!field.type.isPointer && !field.type.namedType.empty()) {
			throw CompileError("struct \"" + decl->name + "\" has a field whose type is itself "
				"a struct, which mslc cannot represent yet (\"" + field.name + "\")");
		}

		const uint32_t components = field.type.vectorWidth > 1 ? field.type.vectorWidth : 1;
		const VectorLayout layout = vectorLayoutFor(
			mappingFor(field.type.scalar).width / 8, components);

		outTypes.push_back(components > 1
			? vector(field.type.scalar, field.type.vectorWidth)
			: scalar(field.type.scalar));

		offset = alignTo(offset, layout.alignment);
		outOffsets.push_back(offset);
		offset += layout.size;

		// The struct's alignment is the largest among its members, and its size is
		// the total rounded up to that, so an array of it steps by a whole number
		// of alignments.
		alignment = std::max(alignment, layout.alignment);
	}

	outSize = alignTo(offset, alignment);
	return true;
}

// A struct as the element of an array of it: the same members with their
// offsets, and no Block decoration. Vulkan requires a struct nested inside a
// Block to be laid out, and rejects a Block-decorated struct inside an array, so
// this is a third form between the value struct and the buffer's block.
Id TypeTable::arrayElementStruct(const std::string& name) {
	const auto cached = _elementStructs.find(name);
	if (cached != _elementStructs.end()) {
		return cached->second;
	}

	std::vector<Id> fieldTypes;
	std::vector<uint32_t> offsets;
	uint32_t size = 0;
	if (!structMembersFor(name, fieldTypes, offsets, size)) {
		return InvalidId;
	}

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, fieldTypes);
	for (size_t i = 0; i < fieldTypes.size(); ++i) {
		_builder.emit(spirv::OpMemberDecorate, { id, static_cast<uint32_t>(i),
			static_cast<uint32_t>(spirv::Decoration::Offset), offsets[i] });
	}

	_elementStructs.emplace(name, id);
	_structSizes.emplace(id, size);
	return id;
}

Id TypeTable::valueStruct(const std::string& name) {
	const auto cached = _valueStructs.find(name);
	if (cached != _valueStructs.end()) {
		return cached->second;
	}

	std::vector<Id> fieldTypes;
	std::vector<uint32_t> offsets;
	uint32_t size = 0;
	if (!structMembersFor(name, fieldTypes, offsets, size)) {
		return InvalidId;
	}

	// Members only. The count is implied by the instruction's word count, and
	// writing it as an operand would be read as one extra member.
	const Id id = _builder.emitDecl(spirv::OpTypeStruct, fieldTypes);

	// No Block and no member offsets: that is the layout of a buffer, and a
	// struct used as a value is laid out by whatever holds it. A Block-decorated
	// struct is only valid in the storage classes a descriptor is allowed in, so
	// using this one as a constant's type would be rejected.
	_valueStructs.emplace(name, id);
	return id;
}

Id TypeTable::namedStruct(const std::string& name) {
	const auto cached = _structs.find(name);
	if (cached != _structs.end()) {
		return cached->second;
	}

	std::vector<Id> fieldTypes;
	std::vector<uint32_t> offsets;
	uint32_t size = 0;
	if (!structMembersFor(name, fieldTypes, offsets, size)) {
		return InvalidId;
	}

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, fieldTypes);

	// A struct reached through a buffer binding is an interface block, so it
	// needs Block and per-member Offset. std140 rules apply, which for the
	// members the corpus has is Metal's own layout.
	_builder.emit(spirv::OpDecorate, { id, static_cast<uint32_t>(spirv::Decoration::Block) });
	for (size_t i = 0; i < fieldTypes.size(); ++i) {
		_builder.emit(spirv::OpMemberDecorate, { id, static_cast<uint32_t>(i),
			static_cast<uint32_t>(spirv::Decoration::Offset), offsets[i] });
	}

	_structs.emplace(name, id);
	return id;
}

void TypeTable::requireCapabilitiesFor(const Type& type) {
	// Float16 and Int64 are emitted where the type is declared, so there is
	// nothing to do here. Kept as a hook rather than removed, because a type
	// needing a capability beyond Shader will need a hook.
	(void)type;
}

//
// Address spaces
//

std::optional<spirv::StorageClassValue> storageClassForAddressSpace(AddressSpace space) {
	switch (space) {
		case AddressSpace::Device: return spirv::StorageClass::StorageBuffer;
		case AddressSpace::Constant: return spirv::StorageClass::Uniform;
		case AddressSpace::Threadgroup: return spirv::StorageClass::Workgroup;
		case AddressSpace::Thread: return spirv::StorageClass::Function;
		case AddressSpace::None: return std::nullopt;
	}

	return std::nullopt;
}

std::vector<const FunctionDecl*> selectEntryPoints(const TranslationUnit& unit, Stage requested) {
	std::vector<const FunctionDecl*> found;

	for (const FunctionDecl& function: unit.functions) {
		// A requested stage narrows the module to that stage, and every function
		// of it is an entry point: Metal's newLibraryWithSource: compiles a whole
		// file, so one .metal source carrying a vertex and a fragment function is
		// one module with two entry points rather than a choice between them.
		if (function.stage == Stage::None) {
			continue;
		}

		if (requested != Stage::None && function.stage != requested) {
			continue;
		}

		found.push_back(&function);
	}

	if (!found.empty()) {
		// Two compute entry points cannot share a module. The workgroup size is
		// three spec constants at SpecId 0, 1, 2, and indium writes constantID
		// 0, 1, 2 once per pipeline against the module it was handed, so there is
		// one set of ids per module and no way to specialise the second entry
		// point. Declaring a second set would leave two decorated constants per
		// id, which spirv-val accepts and no driver can resolve. One vertex and
		// one fragment is the shape indium builds, and it is fine.
		size_t kernels = 0;
		for (const FunctionDecl* function: found) {
			if (function->stage == Stage::Kernel) {
				kernels++;
			}
		}

		if (kernels > 1) {
			throw CompileError("a source may declare one kernel; mslc has no way to give a "
				"second one its own workgroup size, because indium specialises one set of "
				"three constants per module");
		}

		return found;
	}

	if (requested == Stage::None) {
		throw CompileError("source declares no kernel, vertex or fragment entry point");
	}

	throw CompileError(std::string("no ") + (requested == Stage::Kernel ? "kernel"
		: requested == Stage::Vertex ? "vertex" : "fragment")
		+ " entry point in this source");
}

//
// Module emission
//

namespace {

	// A name bound in the function being emitted. The value's SPIR-V type is
	// recorded in the builder, so this only tracks what a name refers to.
	struct Binding {
		Id id = InvalidId;
		// Set when the name denotes a pointer, so indexing it produces an
		// access chain into the pointee rather than a load.
		bool isPointer = false;
		spirv::Id pointeeType = InvalidId;
		// For a buffer parameter, the type a member of the binding-0 block
		// points at, and the member's index. The pointer itself is loaded from
		// the block the first time the parameter is used.
		spirv::Id bufferPointeeType = InvalidId;
		uint32_t memberIndex = 0;
		spirv::StorageClassValue storageClass = spirv::StorageClass::Function;
		// For a struct-typed value, the declaration, so member access can
		// resolve a field.
		const StructDecl* structType = nullptr;
		// What this names points at, as the source spelled it. An access chain
		// walks that rather than the SPIR-V ids, so it can tell a struct from an
		// array from a scalar and find the declaration for the next field.
		Type pointeeMsl;
		// True when the MSL type is a scalar but the SPIR-V form is a vector,
		// so a use wants one component rather than the whole vector.
		bool scalarComponentOfVector = false;
		// True when the name is a descriptor over a wrapped buffer, so indexing
		// it needs a struct member index before the element index.
		bool isBuffer = false;
	};

	// A value rather than a place, so there is no address to load through and a
	// member is taken from the value itself.
	struct ConstantBinding {
		Id id = InvalidId;
		const StructDecl* structType = nullptr;
	};

	// A file-scope "constant" folded to a value. SPIR-V takes only constants as a
	// constant's operands, so an initialiser that refers to an earlier constant
	// or does arithmetic has to be folded here rather than emitted as the
	// expression it was written as.
	struct FoldedConstant {
		// A composite: the type it was declared as, and the constants making it
		// up, which have already been emitted.
		bool isComposite = false;
		Id type = InvalidId;
		std::vector<Id> parts;

		// A scalar: the kind to declare it as, and its value. A double holds
		// every 32-bit integer and every float exactly, which is the whole range
		// of the scalar types a constant here can have.
		ScalarKind scalar = ScalarKind::Void;
		double number = 0.0;
		bool boolean = false;
	};

	class Emitter {
		spirv::Builder& _builder;
		const TranslationUnit& _unit;
		// The module's entry points, in declaration order. Several of them is
		// ordinary: Metal compiles a whole file, so one source carrying a vertex
		// and a fragment function is one module with two entry points.
		const std::vector<const FunctionDecl*>& _entryPoints;
		// The one being emitted. Everything stage-dependent reads it, so a second
		// entry point in another stage gets its own set and its own execution
		// modes without any of them being passed down.
		const FunctionDecl* _entryPoint = nullptr;
		const ModuleOptions& _options;
		TypeTable& _types;

		std::map<std::string, Binding> _bindings;
		// File-scope constants, keyed by name. Separate from _bindings because a
		// constant belongs to the module rather than to the entry point, and an
		// entry point's own names are looked up first.
		std::map<std::string, ConstantBinding> _constants;
		// What each constant folded to, so a later constant referring to this one
		// needs the value rather than a reference to a constant.
		std::map<std::string, FoldedConstant> _folded;
		std::map<spirv::Id, const StructDecl*> _structByValue;
		// The pointee of every buffer parameter, in declaration order, which is
		// what the binding-0 block is built from once the loop is done.
		std::vector<Id> _bufferMembers;
		// The binding-0 block, and the loaded buffer pointer for each member.
		TypeTable::AddressBlock _addressBlock;
		std::map<uint32_t, Id> _bufferBases;

		Id _uintType = InvalidId;
		Id _intType = InvalidId;
		Id _boolType = InvalidId;
		Id _voidType = InvalidId;
		Id _functionId = InvalidId;
		Id _entryPointId = InvalidId;
		bool _glslImported = false;
		spirv::Id _glslSet = InvalidId;

		// Set once the current block has its terminator, so nothing more may be
		// emitted into it until the next label.
		bool _terminated = false;

		std::string _reflection;
		// Ids the entry point lists as its interface, in declaration order.
		std::vector<Id> _interface;

	public:
		Emitter(spirv::Builder& builder, const TranslationUnit& unit,
			const std::vector<const FunctionDecl*>& entryPoints,
			const ModuleOptions& options, TypeTable& types):
			_builder(builder), _unit(unit), _entryPoints(entryPoints),
			_options(options), _types(types) {}

		std::string run();

	private:
		void emitEntryPoint(spirv::Id functionType);
		Id constantU32(uint32_t value);
	// indium splits descriptor sets by stage: set 0 from the vertex function,
	// set 1 from the fragment function. A kernel has one set.
	uint32_t descriptorSet() const {
		return _entryPoint->stage == Stage::Fragment ? 1u : 0u;
	}
		Id bufferBase(const Binding& binding);
		void preloadBufferBases();
		Id loadFromBuffer(Id pointer, Id pointeeType);
		void storeIntoBuffer(Id pointer, Id value);
		// The Aligned memory operand a buffer access has to carry, at the
		// alignment the buffer's own layout guarantees.
		std::vector<uint32_t> alignedOperands(Id pointeeType) const;
		Id declaredTypeOf(const Type& type);
		void declareGlobals();
		Id declareGlobalConstant(const VariableDeclaration& declaration);
		Id emitConstant(const FoldedConstant& folded);
		uint32_t constantBits(ScalarKind kind, double value);
		FoldedConstant foldInitializer(const Type& type, const Expression& initializer);
		void foldStructInitializer(FoldedConstant& folded, const StructDecl& decl,
			const Expression& initializer);
		FoldedConstant foldExpression(const Expression& expression);
		FoldedConstant foldBinary(const Expression& expression);
		FoldedConstant foldUnary(const Expression& expression);
		void declareParameters();
		void emitFunctionBody(const Statement& statement);
		void emitStatement(const Statement& statement);
		void emitExpressionStatement(const Expression& expression);
		void emitVariableDeclaration(const VariableDeclaration& declaration);
		void beginBlock(Id label);
		void terminate(uint16_t opcode, std::vector<uint32_t> operands);
		void branchUnlessTerminated(Id label);

		// Every expression returns a value id whose type the builder knows.
		// For an lvalue such as "buffer[index]" the result is a pointer, and
		// the caller loads from it.
		Id emitExpression(const Expression& expression);
		Id emitBinary(const Expression& expression);
		Id emitUnary(const Expression& expression);
		Id emitIndex(const Expression& expression, bool asAddress);
		Id emitMember(const Expression& expression);
		Id emitMemberAddress(const Expression& expression, Id& outFieldType);

		// The file-scope constant an expression names, or null when it names
		// something else. A constant is a value and not a place, which is what
		// tells a member read on one apart from a member read on a local.
		const ConstantBinding* constantFor(const Expression* expression);
		Id emitCall(const Expression& expression);
		Id emitConstruct(const Expression& expression);
		Id emitIdentifier(const Expression& expression);
		Id loadFrom(Id pointer, Id pointeeType);
		Id convert(Id value, Id fromType, Id toType);
		Id broadcast(Id value, Id vectorType);
	};

	// No setSection here: OpConstant is routed to the types block by the
	// builder, and moving the current section would strand the caller's next
	// instruction in the wrong block.
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
		if (!type.namedType.empty()) {
			const Id id = _types.valueStruct(type.namedType);
			if (id == InvalidId) {
				throw CompileError("undeclared type \"" + type.namedType + "\"");
			}

			return id;
		}

		if (type.vectorWidth > 1) {
			return _types.vector(type.scalar, type.vectorWidth);
		}

		return _types.scalar(type.scalar);
	}


	Id Emitter::loadFrom(Id pointer, Id pointeeType) {
		return _builder.emitTyped(spirv::OpLoad, pointeeType, { pointer });
	}

	// A load or store through a PhysicalStorageBuffer pointer must carry the
	// Aligned memory operand; spirv-val rejects the module without it. There is
	// no exemption for an aggregate: the VUID is about the access, not the type
	// it accesses, and a float3 element is the first aggregate load mslc emits.
	std::vector<uint32_t> Emitter::alignedOperands(Id pointeeType) const {
		return { static_cast<uint32_t>(spirv::MemoryAccess::Aligned),
			_types.alignmentOf(pointeeType) };
	}

	Id Emitter::loadFromBuffer(Id pointer, Id pointeeType) {
		std::vector<uint32_t> operands = alignedOperands(pointeeType);
		operands.insert(operands.begin(), pointer);
		return _builder.emitTyped(spirv::OpLoad, pointeeType, operands);

	}

	void Emitter::storeIntoBuffer(Id pointer, Id value) {
		const Id pointeeType = _types.pointeeOf(_builder.typeOf(pointer));

		// Inserted one at a time: insert(pos, a, b) with two integers is the
		// count-and-value overload, which would insert a copies of b.
		std::vector<uint32_t> operands = alignedOperands(pointeeType);
		operands.insert(operands.begin(), pointer);
		operands.insert(operands.begin() + 1, value);
		_builder.emit(spirv::OpStore, operands);
	}

	// The buffer's own pointer, loaded from its member of the binding-0 block.
	// Loaded once per parameter: the load is what turns the address indium wrote
	// into a pointer the access chain can walk.
	Id Emitter::bufferBase(const Binding& binding) {
		const auto cached = _bufferBases.find(binding.memberIndex);
		if (cached == _bufferBases.end()) {
			throw CompileError("no address was loaded for this buffer, so it cannot be used");
		}

		return cached->second;
	}

	Id Emitter::convert(Id value, Id fromType, Id toType) {
		if (fromType == toType) {
			return value;
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
				? "a scalar cannot be converted to a vector; mslc builds a vector from a "
					"list of values, which it does not do yet"
				: "a vector of " + std::to_string(fromWidth) + " components cannot be "
					"converted to one of " + std::to_string(toWidth) + "; mslc builds a "
					"vector from a list of values, which it does not do yet");
		}

		// A bool is neither floating point nor an integer, and no convert opcode
		// accepts one, so reaching here means the shape is not lowered yet.
		// Reporting it beats emitting an instruction spirv-val rejects.
		if (fromType == _types.scalar(ScalarKind::Bool)
			|| toType == _types.scalar(ScalarKind::Bool)) {
			throw CompileError("converting to or from bool is not lowered yet");
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
			_types.scalar(*temporary), { value });
		return _builder.emitTyped(spirv::OpBitcast, toType, { widened });
	}

	Id Emitter::emitIdentifier(const Expression& expression) {
		// The entry point's own names first, then the module's: a constant declared
		// at file scope is not a parameter of the entry point that reads it.
		const auto it = _bindings.find(expression.name);
		if (it == _bindings.end()) {
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

		const Binding& binding = it->second;

		if (!binding.isPointer) {
			return binding.id;
		}

		// A name bound to a pointer is an lvalue here: the expression's value
		// is what the pointer addresses, so load it. Index and member
		// expressions ask for the address itself, and go through the address
		// path instead.
		const Id loaded = loadFrom(binding.id, binding.pointeeType);

		// A builtin whose MSL type is a scalar but whose SPIR-V form is a
		// vector is that vector's first component: "uint index
		// [[thread_position_in_grid]]" is GlobalInvocationId.x, not the whole
		// vector.
		if (binding.scalarComponentOfVector) {
			const Id scalarType = _types.scalar(ScalarKind::UInt);
			return _builder.emitTyped(spirv::OpCompositeExtract, scalarType, { loaded, 0u });
		}

		return loaded;
	}

	// Indexing is an lvalue: it produces the address of the element. Most uses
	// want the element, so the value is loaded by default and only an
	// assignment target asks for the address. Without this the operands of
	// "a[i] + b[i]" would be pointers, and the addition would be typed as
	// integer.
	Id Emitter::emitIndex(const Expression& expression, bool asAddress) {
		if (expression.left->kind != ExpressionKind::Identifier) {
			throw CompileError("indexing an expression is not supported yet; index a parameter "
				"or local directly");
		}

		const auto it = _bindings.find(expression.left->name);
		if (it == _bindings.end() || !it->second.isPointer) {
			throw CompileError("\"" + expression.left->name + "\" is not a pointer, so it cannot "
				"be indexed");
		}

		const Binding& binding = it->second;

		if (expression.arguments.size() != 1) {
			throw CompileError("expected exactly one index, found " +
				std::to_string(expression.arguments.size()));
		}

		const Id index = emitExpression(*expression.arguments[0]);
		_builder.setType(index, _uintType);

		// A buffer is reached through its address rather than a descriptor, so
		// the access chain starts from the loaded pointer. The result is a
		// pointer in the buffer's own storage class, because the result class of
		// an access chain has to match its base.
		if (binding.bufferPointeeType != InvalidId) {
			const Id base = bufferBase(binding);
			const Id resultType = _types.pointer(spirv::StorageClass::PhysicalStorageBuffer,
				binding.pointeeType);

			// A buffer's pointee is { T runtime_array[] }, so an element is member
			// 0 and then the index. A struct pointee is indexed directly.
			const Id address = binding.isBuffer
				? _builder.emitTyped(spirv::OpAccessChain, resultType,
					{ base, constantU32(0), index })
				: _builder.emitTyped(spirv::OpAccessChain, resultType, { base, index });

			return asAddress ? address : loadFromBuffer(address, binding.pointeeType);
		}

		const Id address = _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(binding.storageClass, binding.pointeeType),
			{ binding.id, index });

		return asAddress ? address : loadFrom(address, binding.pointeeType);
	}

	// The index of a field, or the field count when the struct has no such
	// member. Both member paths need it, and the diagnostic needs the name of
	// what was being asked for.
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

	const ConstantBinding* Emitter::constantFor(const Expression* expression) {
		if (!expression || expression->kind != ExpressionKind::Identifier) {
			return nullptr;
		}

		// An entry point's own name wins: a parameter may be called the same as
		// a constant, and then it is the parameter the source means.
		if (_bindings.count(expression->name) > 0) {
			return nullptr;
		}

		const auto it = _constants.find(expression->name);
		return it == _constants.end() ? nullptr : &it->second;
	}

	// One step of an access chain: either a field of a struct or an element of
	// one, in the order the source wrote them. A field's index is not known until
	// the declaration is, so the name is carried and resolved as the chain is
	// walked.
	struct AccessStep {
		bool isIndex = false;
		const Expression* index = nullptr;
		std::string memberName;
	};

	// Splits "vertices[vid].position" into the name at its root and the steps
	// after it, innermost last, which is the order OpAccessChain wants. An index
	// is part of the chain rather than the end of it: a buffer element's member is
	// reached as "buffer[index].field", and loading the element first would hand
	// OpAccessChain a value where it needs an address.
	static void accessChain(const Expression& expression, std::string& outName,
		std::vector<AccessStep>& outSteps) {

		std::vector<AccessStep> reversed;

		const Expression* current = &expression;
		while (current->kind == ExpressionKind::Member || current->kind == ExpressionKind::Index) {
			AccessStep step;
			step.isIndex = current->kind == ExpressionKind::Index;

			if (step.isIndex) {
				if (current->arguments.size() != 1) {
					throw CompileError("expected exactly one index, found "
						+ std::to_string(current->arguments.size()));
				}

				step.index = current->arguments[0].get();
			} else {
				step.memberName = current->memberName;
			}

			reversed.push_back(step);
			current = current->left.get();
		}

		if (current->kind != ExpressionKind::Identifier) {
			throw CompileError("a member or an index has to start at a name mslc knows");
		}

		outName = current->name;
		outSteps.assign(reversed.rbegin(), reversed.rend());
	}

	Id Emitter::emitMember(const Expression& expression) {
		const ConstantBinding* constant = constantFor(expression.left.get());
		if (constant) {
			if (!constant->structType) {
				throw CompileError("\"" + expression.left->name + "\" is a constant, so \"."
					+ expression.memberName + "\" is not a member of it");
			}

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

		// The chain says which binding the access starts from, which is not the
		// name on the left of the '.' when that is an index.
		std::string rootName;
		std::vector<AccessStep> steps;
		accessChain(expression, rootName, steps);

		Id fieldType = InvalidId;
		const Id address = emitMemberAddress(expression, fieldType);

		// A member read yields the field and a member write needs its address, so
		// the address form does the work and this loads. A buffer's load carries the
		// Aligned operand the same rule as any other access through its pointer.
		const auto it = _bindings.find(rootName);
		return it != _bindings.end() && it->second.bufferPointeeType != InvalidId
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
		accessChain(expression, rootName, steps);

		const auto it = _bindings.find(rootName);
		if (it == _bindings.end()) {
			throw CompileError("\"" + rootName + "\" is not a parameter or local");
		}

		const Binding& binding = it->second;
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

		for (const AccessStep& step: steps) {
			if (step.isIndex) {
				// Only a pointer names a buffer, and only a buffer is an array. A
				// struct reached directly is one value, whether through a reference
				// or through the address a buffer of scalars put in a register.
				if (!fromBuffer || !binding.isBuffer) {
					throw CompileError("\"" + rootName + "\" is not a buffer, so it cannot "
						"be indexed as an array");
				}

				const Id index = emitExpression(*step.index);
				_builder.setType(index, _uintType);
				operands.push_back(index);
				continue;
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
		}

		if (current.namedType.empty() && steps.back().isIndex) {
			throw CompileError("a member access has to end at a struct field");
		}

		outFieldType = declaredTypeOf(current);

		const spirv::StorageClassValue storageClass = fromBuffer
			? spirv::StorageClass::PhysicalStorageBuffer : binding.storageClass;

		// The base is the address the chain starts from: the buffer's own pointer,
		// loaded from the binding-0 block, or the variable itself.
		operands.insert(operands.begin(), fromBuffer ? bufferBase(binding) : binding.id);

		return _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(storageClass, outFieldType), operands);
	}

	Id Emitter::broadcast(Id value, Id vectorType) {
		// A scalar beside a vector is a broadcast, which is what "v * 2.0" and
		// float3(0) both mean in MSL. The scalar is converted to the vector's own
		// component type and put in every component; converting it to the vector
		// type instead would be an OpFConvert into a vector, which no convert
		// opcode accepts, so an int beside a float vector is converted rather than
		// reinterpreted.
		const Id component = _types.componentOf(vectorType);
		const Id widened = convert(value, _builder.typeOf(value), component);
		return _builder.emitTyped(spirv::OpCompositeConstruct, vectorType,
			std::vector<uint32_t>(_types.vectorWidth(vectorType), widened));
	}

	Id Emitter::emitConstruct(const Expression& expression) {
		if (!expression.constructType) {
			throw CompileError("a constructor needs the type it constructs");
		}

		const Type& target = *expression.constructType;
		const std::string spelled = typeName(target);

		// Only a scalar or a vector is a value a constructor produces. A pointer,
		// an array or a struct is not: "float* p(0)" is a null pointer in C++ and
		// not a pointer built from a value, and a local of a pointer type whose
		// initialiser is a scalar stores that scalar into the pointer, which
		// validates and reads as nonsense.
		if (target.isPointer || target.arrayLength || !target.namedType.empty()) {
			throw CompileError("constructing a " + spelled + " is not lowered yet; "
				"mslc builds a scalar or a vector value");
		}

		// An address space on a local is not something mslc can declare. Apple
		// rejects one outright ("automatic variable qualified with an address
		// space"), except for threadgroup, which is legal and needs StorageClass
		// ThreadGroup. mslc declares every local in Function storage, so
		// "threadgroup float3 v(0);" would be a thread-private local in a module
		// whose source says the value is shared, and that validates.
		if (target.addressSpace != AddressSpace::None) {
			throw CompileError("a local in the " + std::string(addressSpaceName(target.addressSpace))
				+ " address space is not lowered yet; every local mslc declares is "
					"thread-private");
		}

		const Id toType = declaredTypeOf(target);

		// float3() and float3(0) are different values and mslc has no value to put
		// in the components, so it says so rather than fabricating a zero. xcrun
		// metal accepts the empty form and zero-fills it; a caller cannot tell
		// that apart from float3(0) in the emitted module, which is the reason for
		// the diagnostic.
		if (expression.arguments.empty()) {
			throw CompileError(spelled + "() has no arguments and mslc has no default "
				"value to put in it; write the value you want, as " + spelled + "(0)");
		}

		// The list form, float4(a, b, c, 1), is a different capability: it builds a
		// vector from one value per component rather than by broadcast.
		if (expression.arguments.size() > 1) {
			throw CompileError(spelled + " built from " + std::to_string(expression.arguments.size())
				+ " values is not lowered yet; one value broadcasts, which is what "
				+ spelled + "(0) does");
		}

		const Id value = emitExpression(*expression.arguments.front());
		const Id fromType = _builder.typeOf(value);

		if (_types.vectorWidth(fromType) == 1 && _types.vectorWidth(toType) > 1) {
			return broadcast(value, toType);
		}

		return convert(value, fromType, toType);
	}

	Id Emitter::emitUnary(const Expression& expression) {
		const Id operand = emitExpression(*expression.left);
		const Id type = _builder.typeOf(operand);

		switch (expression.unaryOperator) {
			case UnaryOperator::Plus: return operand;
			case UnaryOperator::Negate:
				return _types.isFloat(type)
					? _builder.emitTyped(spirv::OpFNegate, type, { operand })
					: _builder.emitTyped(spirv::OpSNegate, type, { operand });
			case UnaryOperator::Not:
				return _builder.emitTyped(spirv::OpLogicalNot, _boolType, { operand });
			case UnaryOperator::BitNot:
				return _builder.emitTyped(spirv::OpNot, type, { operand });
			default:
				throw CompileError("this unary operator is recognised but not lowered yet");
		}
	}

	Id Emitter::emitBinary(const Expression& expression) {
		const Id left = emitExpression(*expression.left);
		const Id right = emitExpression(*expression.right);
		const Id leftType = _builder.typeOf(left);

		const bool isFloat = _types.isFloat(leftType);
		const bool isSigned = _types.isSignedInt(leftType);
		const bool isLogical = expression.binaryOperator == BinaryOperator::LogicalAnd
			|| expression.binaryOperator == BinaryOperator::LogicalOr;

		// A comparison or logical operator yields a bool regardless of operand
		// type; an arithmetic one yields its operand type.
		const bool isComparison = !isLogical && leftType != _boolType
			&& isComparisonOperator(expression.binaryOperator);

		if (isLogical) {
			const uint16_t opcode = expression.binaryOperator == BinaryOperator::LogicalAnd
				? spirv::OpLogicalAnd : spirv::OpLogicalOr;
			return _builder.emitTyped(opcode, _boolType, { left, right });
		}

		if (isComparison) {
			// The two operands have to share a type, and the rule is C's usual
			// arithmetic conversions: a float beats an integer, then the wider
			// integer beats the narrower, and only then does signedness break a tie
			// with both becoming unsigned. Neither direction narrows, which is what
			// this used to do: it converted the right operand to the left one's type,
			// so "3 < f" for a float f turned the 3 into an int and answered the
			// wrong thing, and "b < v" for a uchar and an int turned the int into a
			// uchar. Both validate.
			Id leftOperand = left;
			Id rightOperand = right;
			Id leftType = _builder.typeOf(left);
			Id rightType = _builder.typeOf(right);

			// The integer promotions come before the usual arithmetic conversions,
			// not after them, and skipping them loses a sign. Anything narrower than
			// int becomes int whatever its signedness, so "char a = -1; ushort b = 0;
			// a < b" compares -1 with 0 as two ints and is true. Going straight to the
			// wider type instead widens -1 to 65535, which reads as false.
			//
			// It is only the narrower-than-int types that are wrong, because they are
			// the only ones that fit in an int and so the only ones where the
			// promotion changes the answer. A uint beside an int is already the
			// wider type and the tie-break handles it.
			const bool bothIntegers = !_types.isFloat(leftType) && !_types.isFloat(rightType);
			if (bothIntegers
				&& (_types.bitWidth(leftType) < 32 || _types.bitWidth(rightType) < 32)) {

				const uint32_t components = std::max(_types.vectorWidth(leftType),
					_types.vectorWidth(rightType));
				const Id promoted = components > 1
					? _types.vector(ScalarKind::Int, components)
					: _types.scalar(ScalarKind::Int);

				leftType = promoted;
				rightType = promoted;
				leftOperand = convert(left, _builder.typeOf(left), promoted);
				rightOperand = convert(right, _builder.typeOf(right), promoted);
			}

			const bool leftIsFloat = _types.isFloat(leftType);
			const bool rightIsFloat = _types.isFloat(rightType);
			// Not named `signed`, which is a keyword.
			bool operandsSigned = _types.isSignedInt(leftType);

			// The type both operands end up in, which is also what the opcode is
			// chosen from. Float wins over integer; then the wider integer; then the
			// left one's own type.
			Id commonType = leftType;
			if (leftIsFloat != rightIsFloat) {
				commonType = leftIsFloat ? leftType : rightType;
				operandsSigned = false;
			} else if (_types.bitWidth(rightType) > _types.bitWidth(leftType)) {
				commonType = rightType;
				operandsSigned = _types.isSignedInt(rightType);
			} else if (leftType != rightType) {
				// Equal widths, or the left is already the wider: the tie-break is
				// signedness, and C says both sides become unsigned.
				operandsSigned = _types.isSignedInt(leftType) && _types.isSignedInt(rightType);
			}

			if (_types.isFloat(commonType)) {
				operandsSigned = false;
			} else if (_types.isSignedInt(commonType) != operandsSigned) {
				// Same width, opposite signedness: the signed side becomes the
				// unsigned kind of the same width, which is what makes "-1 < u" an
				// unsigned comparison of 4294967295 against the value.
				// The unsigned kind of the same width, taken from the width rather
				// than named, so a 64-bit operand stays 64 bits.
				uint32_t width = _types.bitWidth(commonType);
				if (width == 64) {
					commonType = _types.scalar(ScalarKind::ULong);
				} else if (width == 16) {
					commonType = _types.scalar(ScalarKind::UShort);
				} else if (width == 8) {
					commonType = _types.scalar(ScalarKind::UChar);
				} else {
					commonType = _types.scalar(ScalarKind::UInt);
				}
			}

			if (_builder.typeOf(left) != commonType) {
				leftOperand = convert(left, _builder.typeOf(left), commonType);
			}
			if (_builder.typeOf(right) != commonType) {
				rightOperand = convert(right, _builder.typeOf(right), commonType);
			}

			return _builder.emitTyped(comparisonOpcode(expression.binaryOperator,
				_types.isFloat(commonType), operandsSigned), _boolType, { leftOperand, rightOperand });
		}

		// OpUDiv and OpUMod need both operands of the result type, and a literal
		// is always uint. A shift count is free to have its own type and width.
		const bool isShift = expression.binaryOperator == BinaryOperator::ShiftLeft
			|| expression.binaryOperator == BinaryOperator::ShiftRight;

		// A scalar beside a vector is a broadcast, which is what "v * 2.0" means
		// in MSL, so the two cases share one rule below.
		const uint32_t rightWidth = _types.vectorWidth(_builder.typeOf(right));
		if (_types.vectorWidth(leftType) > 1 && rightWidth == 1
			&& !isShift && !isLogical && !isComparison) {

			const Id splat = broadcast(right, leftType);
			return _builder.emitTyped(arithmeticOpcode(expression.binaryOperator, isFloat, isSigned),
				leftType, { left, splat });
		}

		const Id rightOperand = isShift ? right : convert(right, _builder.typeOf(right), leftType);
		return _builder.emitTyped(arithmeticOpcode(expression.binaryOperator, isFloat, isSigned),
			leftType, { left, rightOperand });
	}

	Id Emitter::emitCall(const Expression& expression) {
		if (expression.left->kind != ExpressionKind::Identifier) {
			throw CompileError("only a direct function call is supported");
		}

		// The few intrinsics the corpus needs: min and max on scalars, which
		// are GLSL.std.450 rather than core opcodes.
		if (expression.left->name == "min" || expression.left->name == "max") {
			if (expression.arguments.size() != 2) {
				throw CompileError(expression.left->name + " takes two arguments");
			}

			const Id left = emitExpression(*expression.arguments[0]);
			const Id type = _builder.typeOf(left);
			// GLSL.std.450 wants every operand of the result type.
			const Id argument = emitExpression(*expression.arguments[1]);
			const Id right = convert(argument, _builder.typeOf(argument), type);

			if (!_glslImported) {
				_glslImported = true;
				// The set name is a literal string operand, not an OpString id.
				std::vector<uint32_t> name;
				spirv::Builder::appendString(name, "GLSL.std.450");
				_glslSet = _builder.emitDecl(spirv::OpExtInstImport, name);
			}

			return _builder.emitTyped(spirv::OpExtInst, type,
				{ _glslSet, minMaxInstruction(expression.left->name == "max",
					_types.isFloat(type), _types.isSignedInt(type)), left, right });
		}

		throw CompileError("function \"" + expression.left->name + "\" is not a builtin mslc "
			"recognises, and user functions are not lowered yet");
	}

	// The constant a folded value becomes: a scalar as an OpConstant of its kind,
	// a bool as OpConstantTrue or OpConstantFalse, and a composite as the
	// constants making it up.
	Id Emitter::emitConstant(const FoldedConstant& folded) {
		if (folded.isComposite) {
			return _builder.emitDeclTyped(spirv::OpConstantComposite, folded.type, folded.parts);
		}

		if (folded.scalar == ScalarKind::Bool) {
			return folded.boolean
				? _builder.emitDeclTyped(spirv::OpConstantTrue, _boolType, { })
				: _builder.emitDeclTyped(spirv::OpConstantFalse, _boolType, { });
		}

		return _builder.emitDeclTyped(spirv::OpConstant,
			_types.scalar(folded.scalar), { constantBits(folded.scalar, folded.number) });
	}

	// The bits of a scalar constant of the given kind. A float is narrowed to its
	// own width here, since the value was folded as a double and OpConstant takes
	// the bits of the type it declares.
	uint32_t Emitter::constantBits(ScalarKind kind, double value) {
		// A 64-bit constant's literal is two words. Reporting is better than
		// emitting one word of the two, which is a short instruction rather than a
		// narrow constant.
		if (scalarBitWidth(kind) > 32) {
			throw CompileError("a constant of " + std::string(scalarKindName(kind))
				+ " is not lowered yet");
		}

		if (isFloatKind(kind)) {
			const auto narrowed = static_cast<float>(value);
			uint32_t bits = 0;
			static_assert(sizeof(bits) == sizeof(narrowed), "float is not 32 bits");
			std::memcpy(&bits, &narrowed, sizeof(bits));
			return bits;
		}

		return static_cast<uint32_t>(static_cast<int64_t>(value));
	}

	// A struct's fields are initialised by name in Metal, and every one of them
	// has to be given, so the list is checked against the declaration field by
	// field rather than taken positionally.
	void Emitter::foldStructInitializer(FoldedConstant& folded, const StructDecl& decl,
		const Expression& initializer) {

		if (initializer.elements.size() != decl.fields.size()) {
			throw CompileError("struct \"" + decl.name + "\" is initialised with "
				+ std::to_string(initializer.elements.size()) + " values, and it has "
				+ std::to_string(decl.fields.size()) + " fields");
		}

		for (size_t i = 0; i < decl.fields.size(); ++i) {
			const InitializerElement& element = initializer.elements[i];

			if (element.fieldName.empty()) {
				throw CompileError("struct \"" + decl.name + "\" is initialised by field name, so \""
					+ decl.fields[i].name + "\" has to be named rather than given in place");
			}

			if (element.fieldName != decl.fields[i].name) {
				throw CompileError("struct \"" + decl.name + "\" has no field \""
					+ element.fieldName + "\" where \"" + decl.fields[i].name + "\" goes");
			}

			folded.parts.push_back(emitConstant(
				foldInitializer(decl.fields[i].type, *element.value)));
		}
	}

	// An initialiser that is not a list, so a single value. Only what can be
	// worked out from the declarations before it is folded; anything else is
	// reported, since a constant has to be constant.
	FoldedConstant Emitter::foldExpression(const Expression& expression) {
		FoldedConstant folded;

		switch (expression.kind) {
			case ExpressionKind::IntLiteral:
				// An integer literal is an int, as it is in C++, rather than the
				// uint an emitted literal happens to be declared as.
				folded.scalar = ScalarKind::Int;
				folded.number = static_cast<double>(expression.intValue);
				return folded;

			case ExpressionKind::FloatLiteral:
				folded.scalar = ScalarKind::Float;
				folded.number = expression.floatValue;
				return folded;

			case ExpressionKind::BoolLiteral:
				folded.scalar = ScalarKind::Bool;
				folded.boolean = expression.boolValue;
				return folded;

			case ExpressionKind::Identifier: {
				// Declaration order is what makes this resolvable, and the module's
				// constants are declared in the order the source declares them.
				const auto found = _folded.find(expression.name);
				if (found == _folded.end()) {
					throw CompileError("\"" + expression.name + "\" is not a constant this source "
						"declares before this one");
				}

				if (found->second.isComposite) {
					throw CompileError("\"" + expression.name + "\" is a composite constant, and "
						"composites are not folded into a value");
				}

				return found->second;
			}

			case ExpressionKind::Binary: return foldBinary(expression);
			case ExpressionKind::Unary: return foldUnary(expression);
			default: break;
		}

		throw CompileError("a constant's initialiser has to be a value that can be worked out "
			"from the declarations before it, and this is not one");
	}

	FoldedConstant Emitter::foldUnary(const Expression& expression) {
		FoldedConstant folded = foldExpression(*expression.left);

		if (folded.isComposite) {
			throw CompileError("a composite constant cannot have a unary operator applied to it");
		}

		switch (expression.unaryOperator) {
			case UnaryOperator::Plus: return folded;
			case UnaryOperator::Negate:
				folded.number = -folded.number;
				return folded;
			default: break;
		}

		throw CompileError("this unary operator is recognised but not folded yet");
	}

	FoldedConstant Emitter::foldBinary(const Expression& expression) {
		const FoldedConstant left = foldExpression(*expression.left);
		const FoldedConstant right = foldExpression(*expression.right);

		if (left.isComposite || right.isComposite) {
			throw CompileError("a composite constant cannot be an operand of a binary operator");
		}

		if (left.scalar != right.scalar) {
			throw CompileError("a constant's operands have to be of the same type, found "
				+ std::string(scalarKindName(left.scalar)) + " and "
				+ std::string(scalarKindName(right.scalar)));
		}

		const BinaryOperator op = expression.binaryOperator;

		if (isFloatKind(left.scalar)) {
			FoldedConstant folded = left;

			switch (op) {
				case BinaryOperator::Add: folded.number = left.number + right.number; break;
				case BinaryOperator::Subtract: folded.number = left.number - right.number; break;
				case BinaryOperator::Multiply: folded.number = left.number * right.number; break;
				// A float division by zero is infinity, which is what the language
				// says, so it is not treated as a mistake here.
				case BinaryOperator::Divide: folded.number = left.number / right.number; break;
				default:
					throw CompileError("this operator is recognised but not folded yet");
			}

			return folded;
		}

		// Integer arithmetic at the width the operands have, so a constant that
		// overflows wraps the way it would at run time rather than in the double it
		// was folded through.
		const auto l = static_cast<uint32_t>(static_cast<int64_t>(left.number));
		const auto r = static_cast<uint32_t>(static_cast<int64_t>(right.number));

		uint32_t value = 0;
		switch (op) {
			case BinaryOperator::Add: value = l + r; break;
			case BinaryOperator::Subtract: value = l - r; break;
			case BinaryOperator::Multiply: value = l * r; break;
			case BinaryOperator::Modulo:
				if (r == 0) {
					throw CompileError("an integer constant divides by zero");
				}
				value = l % r;
				break;
			case BinaryOperator::Divide:
				if (r == 0) {
					throw CompileError("an integer constant divides by zero");
				}
				value = l / r;
				break;
			case BinaryOperator::ShiftLeft: value = l << (r & 31u); break;
			case BinaryOperator::ShiftRight: value = l >> (r & 31u); break;
			case BinaryOperator::BitAnd: value = l & r; break;
			case BinaryOperator::BitOr: value = l | r; break;
			case BinaryOperator::BitXor: value = l ^ r; break;
			default:
				throw CompileError("this operator is recognised but not folded yet");
		}

		FoldedConstant folded = left;
		folded.number = value;
		return folded;
	}

	// Folds an initialiser of the given declared type. A list of values for a
	// vector or a struct is a composite, and anything else is a scalar, so the
	// declared type is what says which the source wrote.
	FoldedConstant Emitter::foldInitializer(const Type& type, const Expression& initializer) {
		if (initializer.kind != ExpressionKind::InitList) {
			if (!type.namedType.empty()) {
				throw CompileError("the struct \"" + type.namedType + "\" is initialised with a "
					"braced list naming its fields, not with a single value");
			}

			if (type.vectorWidth > 1) {
				throw CompileError("a " + std::string(scalarKindName(type.scalar))
					+ std::to_string(type.vectorWidth) + " constant is initialised with a braced "
					"list of its components, not with a single value");
			}

			FoldedConstant folded = foldExpression(initializer);
			if (folded.isComposite) {
				throw CompileError("a composite cannot initialise a scalar");
			}

			// A bool is initialised with true or false and nothing else. OpConstant
			// has no literal form for it, so folding a number into one would emit
			// a word where the grammar allows none.
			if ((type.scalar == ScalarKind::Bool) != (folded.scalar == ScalarKind::Bool)) {
				throw CompileError(type.scalar == ScalarKind::Bool
					? "a bool constant is initialised with true or false"
					: std::string("a bool cannot initialise a ")
						+ std::string(scalarKindName(type.scalar)));
			}

			// The declared type is what the constant is. An integer literal may
			// initialise a float, since that is a widening; the other direction would
			// have to round, which is not something to do silently.
			if (isFloatKind(folded.scalar) && !isFloatKind(type.scalar)) {
				throw CompileError("a float cannot initialise a "
					+ std::string(scalarKindName(type.scalar)));
			}

			folded.scalar = type.scalar;
			return folded;
		}

		FoldedConstant folded;
		folded.isComposite = true;
		folded.type = declaredTypeOf(type);

		if (type.vectorWidth > 1) {
			// A vector's components are positional, so the list is the vector in
			// order and each element is the vector's own component type.
			if (initializer.elements.size() != type.vectorWidth) {
				throw CompileError("a " + std::string(scalarKindName(type.scalar))
					+ std::to_string(type.vectorWidth) + " constant takes "
					+ std::to_string(type.vectorWidth) + " values, found "
					+ std::to_string(initializer.elements.size()));
			}

			for (const InitializerElement& element: initializer.elements) {
				if (!element.fieldName.empty()) {
					throw CompileError("a vector's components are given in order, but \""
						+ element.fieldName + "\" names one");
				}

				Type component = type;
				component.vectorWidth = 0;
				folded.parts.push_back(emitConstant(foldInitializer(component, *element.value)));
			}

			return folded;
		}

		const StructDecl* decl = _unit.findStruct(type.namedType);
		if (!decl) {
			throw CompileError("a constant of \"" + type.namedType
				+ "\" is not a struct this source declares");
		}

		foldStructInitializer(folded, *decl, initializer);
		return folded;
	}

	// A file-scope "constant" is a compile-time constant, so its initialiser is
	// folded and the result declared as one of the module's constants, with no
	// binding of its own.
	Id Emitter::declareGlobalConstant(const VariableDeclaration& declaration) {
		const FoldedConstant folded = foldInitializer(declaration.type, *declaration.initializer);
		const Id id = emitConstant(folded);

		ConstantBinding binding;
		binding.id = id;
		binding.structType = _unit.findStruct(declaration.type.namedType);
		_constants[declaration.name] = binding;
		_folded[declaration.name] = folded;

		return id;
	}

	// The module's own declarations, in source order. A constant is declared
	// before the entry point that reads it, which is what lets the folder resolve
	// one constant's initialiser against the ones above it.
	void Emitter::declareGlobals() {
		for (const VariableDeclaration& global: _unit.globals) {
			_builder.setSection(spirv::Section::TypesGlobals);
			declareGlobalConstant(global);
		}
	}

	Id Emitter::emitExpression(const Expression& expression) {
		switch (expression.kind) {
			case ExpressionKind::IntLiteral: {
				// The suffix decides, not the value. An unsuffixed integer literal
				// is an int and a `u` one is a uint, which is what picks the
				// opcode for "4294967295u / 3u" and what a literal's conversion
				// reads it through.
				//
				// These were both wrong once and in opposite directions. Emitting
				// every literal as %uint made a negative one wrap: "-1" was
				// OpSNegate %uint %uint_1, and negating 1 as an unsigned is
				// 4294967295. A declaration hid it, because the stored value is
				// bitcast back to %int, so "int b = -1" read correctly while
				// "float3 v(-1)" broadcast 4294967295.0f. Emitting every literal as
				// %int hid nothing and broke unsigned arithmetic instead, because
				// emitBinary takes the opcode from the left type, so
				// "4294967295u / 3u" became OpSDiv.
				//
				// A literal that does not fit in an int is a long in Apple's
				// compiler, with no suffix to ask for it, and mslc has no 64-bit
				// literal. Both reads therefore give the wrong answer for one, and
				// that gap is left for the 64-bit work rather than papered over here.
				const Id type = expression.intIsUnsigned ? _uintType : _intType;
				return _builder.emitDeclTyped(spirv::OpConstant, type,
					{ static_cast<uint32_t>(expression.intValue) });
			}

			case ExpressionKind::FloatLiteral: {
				const auto value = static_cast<float>(expression.floatValue);
				uint32_t bits = 0;
				static_assert(sizeof(bits) == sizeof(value), "float is not 32 bits");
				std::memcpy(&bits, &value, sizeof(bits));
				return _builder.emitDeclTyped(spirv::OpConstant,
					_types.scalar(ScalarKind::Float), { bits });
			}

			case ExpressionKind::BoolLiteral: {
				return expression.boolValue
					? _builder.emitDeclTyped(spirv::OpConstantTrue, _boolType, { })
					: _builder.emitDeclTyped(spirv::OpConstantFalse, _boolType, { });
			}

			case ExpressionKind::Identifier: return emitIdentifier(expression);
			case ExpressionKind::Binary: return emitBinary(expression);
			case ExpressionKind::Unary: return emitUnary(expression);
			case ExpressionKind::Index: return emitIndex(expression, false);
			case ExpressionKind::Member: return emitMember(expression);
			case ExpressionKind::Call: return emitCall(expression);
			case ExpressionKind::Construct: return emitConstruct(expression);
			case ExpressionKind::InitList:
			case ExpressionKind::Assign: break;
		}

		// A braced list is only spelled in an initialiser, and an assignment is a
		// statement, so reaching either here means the source used it as a value,
		// which MSL does not allow.
		throw CompileError("this expression cannot be used as a value");
	}


	// The MSL spelling of a builtin, for a diagnostic that names it.
	const char* builtinName(ParameterAttributes::Builtin builtin) {
		switch (builtin) {
			case ParameterAttributes::Builtin::ThreadPositionInGrid: return "thread_position_in_grid";
			case ParameterAttributes::Builtin::ThreadgroupPositionInGrid:
				return "threadgroup_position_in_grid";
			case ParameterAttributes::Builtin::ThreadPositionInThreadgroup:
				return "thread_position_in_threadgroup";
			case ParameterAttributes::Builtin::ThreadIndexInThreadgroup:
				return "thread_index_in_threadgroup";
			case ParameterAttributes::Builtin::VertexID: return "vertex_id";
			case ParameterAttributes::Builtin::InstanceID: return "instance_id";
			case ParameterAttributes::Builtin::Position: return "position";
			case ParameterAttributes::Builtin::FragCoord: return "frag_coord";
			case ParameterAttributes::Builtin::FrontFacing: return "front_facing";
			case ParameterAttributes::Builtin::None: break;
		}

		return "unknown";
	}

	// Whether Vulkan allows this builtin in this execution model. The dispatch
	// ids are compute only and frag_coord is fragment only, and a module that
	// used one in the wrong stage is rejected by validation rather than being a
	// shader that quietly computes the wrong thing.
	bool builtinAllowedInStage(ParameterAttributes::Builtin builtin, Stage stage) {
		switch (builtin) {
			case ParameterAttributes::Builtin::ThreadPositionInGrid:
			case ParameterAttributes::Builtin::ThreadgroupPositionInGrid:
			case ParameterAttributes::Builtin::ThreadPositionInThreadgroup:
			case ParameterAttributes::Builtin::ThreadIndexInThreadgroup:
				return stage == Stage::Kernel;
			case ParameterAttributes::Builtin::VertexID:
			case ParameterAttributes::Builtin::InstanceID:
				return stage == Stage::Vertex;
			case ParameterAttributes::Builtin::FragCoord:
			case ParameterAttributes::Builtin::FrontFacing:
			case ParameterAttributes::Builtin::Position:
				return stage == Stage::Fragment;
			case ParameterAttributes::Builtin::None: break;
		}

		return false;
	}

	// MSL does not require [[buffer(n)]] on an entry point's device or
	// constant parameters: an unbinding parameter's index is its position
	// among the binding parameters, with builtins skipped. add.metal relies on
	// this, since none of its pointers carry an attribute.
	std::vector<const Parameter*> assignImplicitBindings(const FunctionDecl& entryPoint) {
		std::vector<const Parameter*> bound;

		for (const Parameter& parameter: entryPoint.parameters) {
			if (parameter.attributes.bufferIndex || parameter.attributes.textureIndex
				|| parameter.attributes.samplerIndex || parameter.attributes.builtin) {
				continue;
			}

			bound.push_back(&parameter);
		}

		return bound;
	}

	// Binds every entry point parameter: a builtin or resource attribute gets
	// its own interface variable, and a buffer parameter is folded into a
	// member of the binding-0 address block once every one of them is seen.
	void Emitter::declareParameters() {
		const std::vector<const Parameter*> implicitlyBound = assignImplicitBindings(*_entryPoint);

		for (size_t index = 0; index < _entryPoint->parameters.size(); ++index) {
			const Parameter& parameter = _entryPoint->parameters[index];

			if (parameter.attributes.builtin) {
				const spirv::BuiltInValue spvBuiltin = [&]() {
					switch (*parameter.attributes.builtin) {
						case ParameterAttributes::Builtin::ThreadPositionInGrid:
							return spirv::BuiltIn::GlobalInvocationId;
						case ParameterAttributes::Builtin::ThreadPositionInThreadgroup:
							return spirv::BuiltIn::LocalInvocationId;
						case ParameterAttributes::Builtin::ThreadgroupPositionInGrid:
							return spirv::BuiltIn::WorkgroupId;
						// Vulkan forbids VertexId and InstanceId. The Index forms
						// include the base vertex and instance, as Metal's do.
						case ParameterAttributes::Builtin::VertexID:
							return spirv::BuiltIn::VertexIndex;
						case ParameterAttributes::Builtin::InstanceID:
							return spirv::BuiltIn::InstanceIndex;
						case ParameterAttributes::Builtin::FragCoord:
							return spirv::BuiltIn::FragCoord;
						case ParameterAttributes::Builtin::FrontFacing:
							return spirv::BuiltIn::FrontFacing;
						default:
							throw CompileError("this builtin is recognised but not lowered yet");
					}
				}();

				// Vulkan fixes each builtin's type. The compute ids are always
				// uvec3 even where MSL declares a narrower width, so a scalar
				// declaration extracts a component.
				Id typeId = InvalidId;
				switch (spvBuiltin) {
					case spirv::BuiltIn::VertexIndex:
					case spirv::BuiltIn::InstanceIndex:
						typeId = _types.scalar(ScalarKind::UInt);
						break;
					case spirv::BuiltIn::FragCoord:
						typeId = _types.vector(ScalarKind::Float, 4);
						break;
					case spirv::BuiltIn::FrontFacing:
						typeId = _boolType;
						break;
					default:
						typeId = _types.vector(ScalarKind::UInt, 3);
						break;
				}
				_builder.setSection(spirv::Section::TypesGlobals);
				const Id pointerType = _types.pointer(spirv::StorageClass::Input, typeId);
				const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
					{ static_cast<uint32_t>(spirv::StorageClass::Input) });

				// Vulkan fixes which execution models each builtin is legal in, and
				// an integer Input on a fragment entry point additionally has to be
				// Flat. A builtin used in the wrong stage is rejected rather than
				// decorated: the alternative is a module that validates on one
				// stage's terms and is wrong on another's. Checked before the
				// decoration, so a rejected builtin is never emitted.
				if (!builtinAllowedInStage(*parameter.attributes.builtin, _entryPoint->stage)) {
					throw CompileError(std::string("builtin \"") + builtinName(*parameter.attributes.builtin)
						+ "\" is not available in a "
						+ (_entryPoint->stage == Stage::Kernel ? "kernel"
							: _entryPoint->stage == Stage::Vertex ? "vertex" : "fragment")
						+ " function (parameter \"" + parameter.name + "\")");
				}

				_builder.setSection(spirv::Section::Annotations);
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(spvBuiltin) });

				// An integer Input on a fragment entry point has to be Flat:
				// there is no sensible way to interpolate an integer, and
				// VUID-StandaloneSpirv-Flat-04744 rejects the module without it.
				// A compute or vertex entry point has no interpolation at all, so
				// the decoration would be meaningless there.
				if (_entryPoint->stage == Stage::Fragment && spvBuiltin != spirv::BuiltIn::FragCoord
					&& spvBuiltin != spirv::BuiltIn::FrontFacing) {
					_builder.emit(spirv::OpDecorate, { id,
						static_cast<uint32_t>(spirv::Decoration::Flat) });
				}

				Binding binding;
				binding.id = id;
				binding.isPointer = true;
				binding.pointeeType = typeId;
				binding.storageClass = spirv::StorageClass::Input;
				binding.scalarComponentOfVector = parameter.type.isScalar()
					&& _types.vectorWidth(typeId) > 1;
				_bindings[parameter.name] = binding;
				_interface.push_back(id);
				continue;
			}

			if (parameter.attributes.textureIndex || parameter.attributes.samplerIndex) {
				throw CompileError("texture and sampler parameters are not lowered yet (parameter \""
					+ parameter.name + "\")");
			}

			// An unbinding parameter takes the next index among the binding
			// parameters, which is what Metal does.
			uint32_t bindingIndex = 0;
			if (parameter.attributes.bufferIndex) {
				bindingIndex = *parameter.attributes.bufferIndex;
			} else {
				const auto it = std::find(implicitlyBound.begin(), implicitlyBound.end(), &parameter);
				if (it == implicitlyBound.end()) {
					throw CompileError("parameter \"" + parameter.name + "\" has no [[buffer]], "
						"[[texture]], [[sampler]] or builtin attribute, and no address space mslc "
						"can bind, so there is nothing to bind it to");
				}
				bindingIndex = static_cast<uint32_t>(it - implicitlyBound.begin());
			}

		if (!storageClassForAddressSpace(parameter.type.addressSpace)) {
			throw CompileError("parameter \"" + parameter.name + "\" needs a device, constant "
				"or threadgroup address space to be a buffer binding");
		}

		Id pointeeType = InvalidId;
		const StructDecl* structType = nullptr;

		if (!parameter.type.namedType.empty()) {
			// A Metal "device T*" is a buffer of T, so the element is the struct
			// laid out for an array rather than the Block-decorated form: Vulkan
			// requires a struct nested inside a Block to be laid out, and rejects a
			// Block-decorated struct inside an array. A struct reached directly is
			// the buffer itself, so it is the Block form.
			if (parameter.type.isPointer) {
				pointeeType = _types.arrayElementStruct(parameter.type.namedType);
			} else {
				pointeeType = _types.namedStruct(parameter.type.namedType);
			}

			if (pointeeType == InvalidId) {
				throw CompileError("parameter \"" + parameter.name + "\" refers to undeclared "
					"type \"" + parameter.type.namedType + "\"");
			}

			structType = _unit.findStruct(parameter.type.namedType);
		} else {
			pointeeType = declaredTypeOf(parameter.type);
		}

		// A buffer parameter's address is a member of the binding-0 block rather
		// than a descriptor of its own, so what a member points at is the
		// buffer's own type. A pointer parameter is a whole buffer, and
		// OpAccessChain rejects a non-composite base, so the element type is
		// wrapped in { T runtime_array[] } and the parameter indexes through
		// that -- for a struct element as much as for a scalar one.
		Id bufferPointee = pointeeType;
		bool isBuffer = false;
		if (parameter.type.isPointer) {
			bufferPointee = _types.blockStructFor(pointeeType);
			isBuffer = true;
		}

		// The member index is this parameter's position among the buffer
		// parameters in declaration order, which is what indium fills in. It is
		// not the Metal index: that says which buffer the app bound there, and
		// goes in the reflection.
		const auto memberIndex = static_cast<uint32_t>(_bufferMembers.size());
		_bufferMembers.push_back(bufferPointee);

		// The block is emitted after the loop, once every member is known.
		Binding binding;
		binding.isPointer = true;
		binding.pointeeType = pointeeType;
		binding.bufferPointeeType = bufferPointee;
		binding.storageClass = spirv::StorageClass::PhysicalStorageBuffer;
		binding.structType = structType;
		binding.isBuffer = isBuffer;
		binding.pointeeMsl = parameter.type;
		binding.pointeeMsl.isPointer = false;
		binding.pointeeMsl.isConst = false;
		binding.memberIndex = memberIndex;
		_bindings[parameter.name] = binding;

		// The Metal index says which buffer the app bound at that slot. Every
		// buffer of an entry point shares one block at binding 0, so it is not a
		// per-buffer descriptor binding and the reflection says which one instead.
		// The set is the one the block is actually declared in, which is 1 for a
		// fragment entry point.
		//
		// The separator goes before the entry rather than after it, because a
		// trailing comma makes the array a syntax error: json.load refuses
		// "Illegal trailing comma before end of array", so a reflection a reader
		// cannot parse is not a reflection. That is why the first binding is the
		// one that carries no comma.
		if (!_reflection.empty()) {
			_reflection.pop_back();  // the newline the previous entry ended with
			_reflection += ",\n";
		}

		_reflection += "\t\t\t\t{ \"kind\": \"Buffer\", \"metal_index\": "
			+ std::to_string(bindingIndex)
			+ ", \"descriptor\": { \"set\": " + std::to_string(descriptorSet())
			+ ", \"binding\": 0 }"
			+ ", \"member\": " + std::to_string(memberIndex)
			+ ", \"param_index\": " + std::to_string(index)
			+ ", \"name\": \"" + parameter.name + "\" }\n";
	}

		// One block for every buffer parameter, at binding 0. It is emitted here
		// rather than per parameter because its member list is only known once the
		// loop is done, and indium binds it as a single uniform buffer whose
		// entries are the buffer addresses in this order.
		if (!_bufferMembers.empty()) {
			_addressBlock = _types.addressBlock(_bufferMembers, descriptorSet());
			_interface.push_back(_addressBlock.variable);
		}
	}
	// Each buffer's address is loaded once, here, so that the value dominates
	// every use. A load emitted at the first use would not dominate a later use
	// in another basic block, and spirv-val rejects a module where an id is used
	// outside the block that defined it.
	void Emitter::preloadBufferBases() {
		for (size_t k = 0; k < _bufferMembers.size(); ++k) {
			const Id memberType = _types.bufferPointer(_bufferMembers[k]);
			const Id memberAddress = _builder.emitTyped(spirv::OpAccessChain,
				_types.pointer(spirv::StorageClass::Uniform, memberType),
				{ _addressBlock.variable, constantU32(static_cast<uint32_t>(k)) });
			_bufferBases.emplace(static_cast<uint32_t>(k),
				_builder.emitTyped(spirv::OpLoad, memberType, { memberAddress }));
		}
	}


	// A local variable becomes a Function-storage pointer, which the
	// expression path then loads from, so a local and a parameter behave the
	// same way when a name is used.
	void Emitter::emitVariableDeclaration(const VariableDeclaration& declaration) {
		// A local has to be a value: the declaration stores an initialiser into a
		// Function-storage variable, and declaredTypeOf answers a pointer type with
		// its pointee, so a local of pointer or array type would be declared as that
		// base and have the initialiser converted to it. "float *p = 0;" stored a
		// float 0.0 into a pointer and "float[2] v;" declared one float, both of
		// which validate. The type is quoted as the source spelled it, so the
		// message names "float *" rather than "float".
		if (declaration.type.isPointer) {
			throw CompileError("a local of type " + typeName(declaration.type)
				+ " is not lowered yet, since a pointer is not a value mslc can declare "
					"in a function's own storage");
		}

		if (declaration.type.arrayLength) {
			throw CompileError("a local of array type " + typeName(declaration.type)
				+ " is not lowered yet; an array has no single SPIR-V type to store into");
		}

		const Id typeId = declaredTypeOf(declaration.type);

		Id initial = InvalidId;
		if (declaration.initializer) {
			const Id initializer = emitExpression(*declaration.initializer);
			initial = convert(initializer, _builder.typeOf(initializer), typeId);
		} else {
			initial = _types.zero(typeId);
		}

		const Id pointerType = _types.pointer(spirv::StorageClass::Function, typeId);
		const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });
		_builder.emit(spirv::OpStore, { id, initial });

		Binding binding;
		binding.id = id;
		binding.isPointer = true;
		binding.pointeeType = typeId;
		binding.storageClass = spirv::StorageClass::Function;
		_bindings[declaration.name] = binding;
	}

	// A label's id is allocated ahead of time so a branch can name it, which
	// is why this emits the label with its own id instead of emitDecl's.
	void Emitter::beginBlock(Id label) {
		_builder.emit(spirv::OpLabel, { label });
		_terminated = false;
	}

	void Emitter::terminate(uint16_t opcode, std::vector<uint32_t> operands) {
		_builder.emit(opcode, std::move(operands));
		_terminated = true;
	}

	// A block that already returned must not also branch to the merge.
	void Emitter::branchUnlessTerminated(Id label) {
		if (!_terminated) {
			terminate(spirv::OpBranch, { label });
		}
	}

	void Emitter::emitExpressionStatement(const Expression& expression) {
		// An assignment yields an address rather than a value, so it is
		// handled here rather than through emitExpression.
		if (expression.kind != ExpressionKind::Assign) {
			emitExpression(expression);
			return;
		}

		// The left side is a place, so it is emitted as an address rather
		// than loaded.
		Id address = InvalidId;
		if (expression.left->kind == ExpressionKind::Index) {
			address = emitIndex(*expression.left, true);
		} else if (expression.left->kind == ExpressionKind::Member) {
			Id fieldType = InvalidId;
			address = emitMemberAddress(*expression.left, fieldType);
		} else if (expression.left->kind == ExpressionKind::Identifier) {
			// A local names a Function-storage pointer, so the variable itself is
			// the address to store to. A buffer parameter is a descriptor and a
			// plain value is loaded, so only this case takes the binding.
			const auto it = _bindings.find(expression.left->name);
			if (it != _bindings.end() && it->second.isPointer
				&& it->second.storageClass == spirv::StorageClass::Function) {
				address = it->second.id;
			} else {
				address = emitExpression(*expression.left);
			}
		} else {
			address = emitExpression(*expression.left);
		}

		const Id value = emitExpression(*expression.right);

		// A store's value has the type the address points at, not the type of
		// the address, so the pointee is what the value is converted to.
		const Id addressType = _builder.typeOf(address);
		const Id pointeeType = _types.pointeeOf(addressType);
		if (pointeeType == spirv::InvalidId) {
			throw CompileError("cannot determine what \""
				+ expression.left->name + "\" points at, so the store cannot be typed");
		}

		// A store into a buffer carries the Aligned memory operand, which the
		// same layout rule gives the access.
		const auto storageClass = _types.storageClassOf(addressType);
		if (storageClass && *storageClass == spirv::StorageClass::PhysicalStorageBuffer) {
			storeIntoBuffer(address, convert(value, _builder.typeOf(value), pointeeType));
			return;
		}

		_builder.emit(spirv::OpStore, { address,
			convert(value, _builder.typeOf(value), pointeeType) });
	}

	void Emitter::emitStatement(const Statement& statement) {
		switch (statement.kind) {
			case StatementKind::Compound:
				// Whatever follows a return in the same block is unreachable,
				// and a block holds nothing after its terminator.
				for (const StatementPtr& child: statement.children) {
					if (_terminated) {
						break;
					}
					emitStatement(*child);
				}
				return;

			case StatementKind::ExpressionStatement:
				if (statement.expression) {
					emitExpressionStatement(*statement.expression);
				}
				return;

			case StatementKind::DeclarationStatement:
				emitVariableDeclaration(*statement.declaration);
				return;

			case StatementKind::Return:
				if (statement.expression) {
					terminate(spirv::OpReturnValue, { emitExpression(*statement.expression) });
				} else {
					terminate(spirv::OpReturn, { });
				}
				return;

			// Vulkan requires structured control flow: every conditional branch
			// is preceded by a merge instruction naming where its paths rejoin.
			case StatementKind::If: {
				const Id condition = emitExpression(*statement.expression);
				const Id thenLabel = _builder.nextId();
				const Id mergeLabel = _builder.nextId();
				const Id elseLabel = statement.elseBranch ? _builder.nextId() : mergeLabel;

				_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
				terminate(spirv::OpBranchConditional, { condition, thenLabel, elseLabel });

				beginBlock(thenLabel);
				emitStatement(*statement.thenBranch);
				branchUnlessTerminated(mergeLabel);

				if (statement.elseBranch) {
					beginBlock(elseLabel);
					emitStatement(*statement.elseBranch);
					branchUnlessTerminated(mergeLabel);
				}

				beginBlock(mergeLabel);
				return;
			}

			// Loops share one shape: the header evaluates the condition and
			// declares the merge and continue targets, the body branches to the
			// continue block, and that block alone branches back to the header.
			case StatementKind::For:
			case StatementKind::While: {
				const bool isFor = statement.kind == StatementKind::For;

				if (isFor && statement.forInitializer) {
					emitVariableDeclaration(*statement.forInitializer);
				} else if (isFor && statement.expression) {
					emitExpressionStatement(*statement.expression);
				}

				const Expression* condition = isFor
					? statement.forCondition.get() : statement.whileCondition.get();
				const Id headerLabel = _builder.nextId();
				const Id bodyLabel = _builder.nextId();
				const Id continueLabel = _builder.nextId();
				const Id mergeLabel = _builder.nextId();

				terminate(spirv::OpBranch, { headerLabel });
				beginBlock(headerLabel);
				const Id conditionValue = condition ? emitExpression(*condition) : InvalidId;
				_builder.emit(spirv::OpLoopMerge, { mergeLabel, continueLabel, kLoopControlNone });
				if (condition) {
					terminate(spirv::OpBranchConditional, { conditionValue, bodyLabel, mergeLabel });
				} else {
					terminate(spirv::OpBranch, { bodyLabel });
				}

				beginBlock(bodyLabel);
				emitStatement(isFor ? *statement.forBody : *statement.whileBody);
				branchUnlessTerminated(continueLabel);

				beginBlock(continueLabel);
				if (isFor && statement.forIncrement) {
					emitExpressionStatement(*statement.forIncrement);
				}
				terminate(spirv::OpBranch, { headerLabel });

				beginBlock(mergeLabel);
				return;
			}

			case StatementKind::Break:
			case StatementKind::Continue:
			case StatementKind::Discard:
				throw CompileError("break, continue and discard_fragment inside a loop are "
					"recognised but not lowered yet");
		}
	}

	void Emitter::emitFunctionBody(const Statement& statement) {
		emitStatement(statement);
	}

	// Emits one entry point: its parameters, its OpEntryPoint, its execution
	// modes and its function. Everything stage-dependent is decided from
	// _entryPoint, which the caller points at the one being emitted. functionType
	// is the module's one void function type.
	void Emitter::emitEntryPoint(Id functionType) {
		declareParameters();

		spirv::ExecutionModelValue model = spirv::ExecutionModel::GLCompute;
		if (_entryPoint->stage == Stage::Vertex) {
			model = spirv::ExecutionModel::Vertex;
		} else if (_entryPoint->stage == Stage::Fragment) {
			model = spirv::ExecutionModel::Fragment;
		}

		// The entry point's own id has to exist before it is named in
		// OpEntryPoint, so it is allocated here rather than after.
		_entryPointId = _builder.nextId();

		_builder.setSection(spirv::Section::EntryPoints);
		{
			std::vector<uint32_t> operands = {
				static_cast<uint32_t>(model), _entryPointId
			};
			spirv::Builder::appendString(operands, _entryPoint->name);
			for (Id id: _interface) {
				operands.push_back(id);
			}
			_builder.emit(spirv::OpEntryPoint, operands);
		}

		_builder.setSection(spirv::Section::ExecutionModes);
		if (_entryPoint->stage == Stage::Kernel) {
			// The workgroup size is a spec constant rather than a literal, so
			// indium can supply Metal's threadsPerThreadgroup through
			// VkSpecializationInfo at pipeline creation. A literal would be baked
			// in and the value the app asked for would be ignored.
			//
			// One set of SpecIds for the module, not one per entry point. indium
			// writes constantID 0, 1, 2 once per pipeline against the module it was
			// given, so a second compute entry point declaring its own 0, 1, 2 would
			// leave two decorated constants per id, which spirv-val accepts and no
			// driver can resolve: both entry points would read whichever the driver
			// picked. A module with more than one compute entry point is refused
			// below rather than given a second set.
			const Id x = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType,
				{ _options.localSizeX });
			const Id y = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType,
				{ _options.localSizeY });
			const Id z = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType,
				{ _options.localSizeZ });
			_builder.emit(spirv::OpDecorate, { x,
				static_cast<uint32_t>(spirv::Decoration::SpecId), 0u });
			_builder.emit(spirv::OpDecorate, { y,
				static_cast<uint32_t>(spirv::Decoration::SpecId), 1u });
			_builder.emit(spirv::OpDecorate, { z,
				static_cast<uint32_t>(spirv::Decoration::SpecId), 2u });

			// OpExecutionModeId, not OpExecutionMode: a mode whose extra operands
			// are ids has its own instruction, and OpExecutionMode rejects id
			// operands outright.
			//
			// The section is set again here because the decorations above moved it
			// to the annotations, and an execution mode emitted from there lands
			// in the wrong section and is ignored.
			_builder.setSection(spirv::Section::ExecutionModes);
			_builder.emit(spirv::OpExecutionModeId, { _entryPointId,
				static_cast<uint32_t>(spirv::ExecutionMode::LocalSizeId), x, y, z });
		}

		if (_entryPoint->stage == Stage::Fragment) {
			// Vulkan requires one of the two origin modes on a fragment entry
			// point, and Metal's framebuffer origin is upper left.
			_builder.setSection(spirv::Section::ExecutionModes);
			_builder.emit(spirv::OpExecutionMode, { _entryPointId,
				static_cast<uint32_t>(spirv::ExecutionMode::OriginUpperLeft) });
		}

		_builder.setSection(spirv::Section::Functions);
		// The function's own id has to be the one the entry point names, so it
		// is written explicitly rather than taken from the emit's return.
		_builder.emitDeclTypedAt(spirv::OpFunction, _voidType, _entryPointId,
			{ kFunctionControlNone, functionType });
		beginBlock(_builder.nextId());
		preloadBufferBases();
		emitFunctionBody(*_entryPoint->body);
		if (!_terminated) {
			terminate(spirv::OpReturn, { });
		}
		_builder.emit(spirv::OpFunctionEnd, { });
	}

	// Emits the whole module: the capabilities, the memory model and the
	// file-scope globals once, then every entry point. Returns the reflection
	// document.
	std::string Emitter::run() {
		_builder.setSection(spirv::Section::Capabilities);
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Shader) });

	// The capability goes with the addressing model rather than with the block:
	// the model is declared whether or not the shader has a buffer, so a kernel
	// with no buffer parameter would otherwise name a capability it never
	// declared.
	_builder.setSection(spirv::Section::Capabilities);
	_builder.emit(spirv::OpCapability,
		{ static_cast<uint32_t>(spirv::Capability::PhysicalStorageBufferAddresses) });

	_builder.setSection(spirv::Section::MemoryModel);
	// PhysicalStorageBuffer64, because a buffer is reached by the address the
	// binding-0 block hands the shader rather than by a descriptor of its own.
	_builder.emit(spirv::OpMemoryModel,
		{ static_cast<uint32_t>(spirv::AddressingModel::PhysicalStorageBuffer64),
		  static_cast<uint32_t>(spirv::MemoryModel::GLSL450) });


		_uintType = _types.scalar(ScalarKind::UInt);
		_intType = _types.scalar(ScalarKind::Int);
		_boolType = _types.scalar(ScalarKind::Bool);
		_voidType = _types.voidType();

		// Before the parameters, since a constant's initialiser may name a struct
		// and a struct's members have to be declared before the constant that holds
		// one of them.
		declareGlobals();

		// The function type, declared once for the module. Every entry point is a
		// void function, and a duplicate OpTypeFunction is a hard validation
		// failure, so this cannot be emitted per entry point.
		_builder.setSection(spirv::Section::TypesGlobals);
		// No parameter count: like OpTypeStruct, the trailing variadic count is
		// derived from the instruction's word count and is not in the binary.
		// Writing one is read as a parameter type, which shows up as "Id is 0".
		const Id functionType = _builder.emitDecl(spirv::OpTypeFunction, { _voidType });

		// One reflection entry per entry point, in the order they were selected.
		std::string document = "{\n";
		document += "\t\"reflection_version\": 2,\n";
		document += "\t\"entry_points\": [\n";

		for (size_t k = 0; k < _entryPoints.size(); ++k) {
			_entryPoint = _entryPoints[k];

			// Per entry point rather than per module: a name bound in one
			// function is not visible in another, the buffers are that entry
			// point's own, and the interface is the list this OpEntryPoint names.
			_bindings.clear();
			_bufferMembers.clear();
			_bufferBases.clear();
			_interface.clear();
			_reflection.clear();
			// The address block is per entry point too, and clearing it with the
			// rest is what keeps a later change to when it is read from inheriting
			// the previous function's. Nothing reads it while _bufferMembers is
			// empty today, so this is not a fix for an observable case.
			_addressBlock = {};
			_terminated = false;

			emitEntryPoint(functionType);

			document += "\t\t{\n";
			document += "\t\t\t\"name\": \"" + _entryPoint->name + "\",\n";
			document += std::string("\t\t\t\"stage\": \"")
				+ (_entryPoint->stage == Stage::Kernel ? "compute"
					: _entryPoint->stage == Stage::Vertex ? "vertex" : "fragment")
				+ "\",\n";
			document += "\t\t\t\"local_size\": [" + std::to_string(_options.localSizeX) + ", "
				+ std::to_string(_options.localSizeY) + ", "
				+ std::to_string(_options.localSizeZ) + "],\n";
			document += "\t\t\t\"bindings\": [\n";
			document += _reflection;
			document += "\t\t\t]\n";
			document += k + 1 < _entryPoints.size() ? "\t\t},\n" : "\t\t}\n";
		}

		document += "\t]\n}\n";

		return document;
	}

}

std::string emitModule(spirv::Builder& builder, const TranslationUnit& unit,
	const std::vector<const FunctionDecl*>& entryPoints, const ModuleOptions& options) {
	TypeTable types(builder, unit);
	Emitter emitter(builder, unit, entryPoints, options, types);
	return emitter.run();
}

}
