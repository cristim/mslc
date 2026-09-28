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

	bool isSignedInteger(ScalarKind kind) {
		return kind == ScalarKind::Char || kind == ScalarKind::Short
			|| kind == ScalarKind::Int || kind == ScalarKind::Long;
	}

	bool isFloatKind(ScalarKind kind) {
		return mappingFor(kind).isFloat;
	}

	// MSL spells the components of a vector as field-like names, and the colour
	// sets are the same components under other letters. Returns the component
	// indices a name selects, or nothing when the name is not one.
	std::optional<std::vector<uint32_t>> swizzleIndices(const std::string& name) {
		if (name.empty() || name.size() > 4) {
			return std::nullopt;
		}

		std::vector<uint32_t> indices;
		for (const char letter: name) {
			uint32_t index;
			switch (letter) {
				case 'x': case 'r': case 's': index = 0; break;
				case 'y': case 'g': case 't': index = 1; break;
				case 'z': case 'b': case 'p': index = 2; break;
				case 'w': case 'a': case 'q': index = 3; break;
				default: return std::nullopt;
			}

			indices.push_back(index);
		}

		return indices;
	}

	// What a builtin parameter is declared as: the SPIR-V builtin, and the type
	// it has to be declared with. Vulkan fixes the width of each and rejects it
	// at any other, so a workgroup or invocation id is three components while a
	// vertex or instance index is one.
	struct BuiltinInput {
		spirv::BuiltInValue builtin;
		ScalarKind scalar;
		uint32_t width;
	};

	BuiltinInput builtinInputFor(ParameterAttributes::Builtin builtin) {
		switch (builtin) {
			case ParameterAttributes::Builtin::ThreadPositionInGrid:
				return { spirv::BuiltIn::GlobalInvocationId, ScalarKind::UInt, 3 };
			case ParameterAttributes::Builtin::ThreadPositionInThreadgroup:
				return { spirv::BuiltIn::LocalInvocationId, ScalarKind::UInt, 3 };
			case ParameterAttributes::Builtin::ThreadgroupPositionInGrid:
				return { spirv::BuiltIn::WorkgroupId, ScalarKind::UInt, 3 };

			// VertexIndex rather than VertexId, and InstanceIndex rather than
			// InstanceId: Vulkan reserves the two with the Id ending for mesh
			// shaders and rejects them anywhere else.
			case ParameterAttributes::Builtin::VertexID:
				return { spirv::BuiltIn::VertexIndex, ScalarKind::UInt, 1 };
			case ParameterAttributes::Builtin::InstanceID:
				return { spirv::BuiltIn::InstanceIndex, ScalarKind::UInt, 1 };

			case ParameterAttributes::Builtin::FragCoord:
				return { spirv::BuiltIn::FragCoord, ScalarKind::Float, 4 };
			case ParameterAttributes::Builtin::FrontFacing:
				return { spirv::BuiltIn::FrontFacing, ScalarKind::Bool, 1 };

			case ParameterAttributes::Builtin::Position:
			case ParameterAttributes::Builtin::None:
				break;
		}

		throw CompileError("this builtin is recognised but not lowered yet");
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

	// The SPIR-V opcode for a binary operation, given whether the operands are
	// floating point. SPIR-V has separate opcodes for each, so the choice has
	// to be made from the resolved operand type rather than from the AST,
	// which does not record it.
	uint16_t arithmeticOpcode(BinaryOperator op, bool isFloat) {
		using Op = uint16_t;
#define BOTH(f, i) ((isFloat) ? Op(f) : Op(i))

		switch (op) {
			case BinaryOperator::Add: return BOTH(spirv::OpFAdd, spirv::OpIAdd);
			case BinaryOperator::Subtract: return BOTH(spirv::OpFSub, spirv::OpISub);
			case BinaryOperator::Multiply: return BOTH(spirv::OpFMul, spirv::OpIMul);
			case BinaryOperator::Divide: return BOTH(spirv::OpFDiv, spirv::OpSDiv);
			case BinaryOperator::Modulo: return BOTH(spirv::OpFMod, spirv::OpSMod);
			case BinaryOperator::BitAnd: return spirv::OpBitwiseAnd;
			case BinaryOperator::BitOr: return spirv::OpBitwiseOr;
			case BinaryOperator::BitXor: return spirv::OpBitwiseXor;
			case BinaryOperator::ShiftLeft: return spirv::OpShiftLeftLogical;
			case BinaryOperator::ShiftRight: return spirv::OpShiftRightLogical;
			default: break;
		}

#undef BOTH

		throw CompileError("this binary operator is recognised but not lowered yet");
	}

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

	// One of MSL's builtin math functions and the GLSL.std.450 instruction it
	// lowers to. Each is spelled the same way in both, with the operands in the
	// same order, so the lowering is the source's arguments passed through behind
	// the instruction set.
	//
	// The instruction numbers are generated from the extended instruction grammar
	// rather than written out here, because a wrong one is a module that validates
	// and computes the wrong thing: GLSL.std.450 has both FMin and FMax, and
	// swapping the two names is silent.
	struct BuiltinMath {
		spirv::InstructionValue instruction;
		size_t arguments;
	};

	std::optional<BuiltinMath> builtinMathFor(const std::string& name) {
		static const std::map<std::string, BuiltinMath> table = {
			{ "min", { spirv::glsl450::FMin, 2 } },
			{ "max", { spirv::glsl450::FMax, 2 } },
			{ "normalize", { spirv::glsl450::Normalize, 1 } },
			{ "pow", { spirv::glsl450::Pow, 2 } },
			{ "reflect", { spirv::glsl450::Reflect, 2 } },
			{ "refract", { spirv::glsl450::Refract, 3 } },
		};

		const auto found = table.find(name);
		if (found == table.end()) {
			return std::nullopt;
		}

		return found->second;
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

Id TypeTable::sampler() {
	if (_samplerType != InvalidId) {
		return _samplerType;
	}

	_samplerType = _builder.emitDecl(spirv::OpTypeSampler);
	return _samplerType;
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
	if (mapping.isFloat) {
		if (mapping.width == 16) {
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Float16) });
		}
		if (mapping.width == 64) {
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Int64) });
		}
		id = _builder.emitDecl(spirv::OpTypeFloat, { mapping.width });
	} else if (kind == ScalarKind::Bool) {
		id = _builder.emitDecl(spirv::OpTypeBool);
	} else {
		if (mapping.width == 64) {
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Int64) });
		}
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

Id TypeTable::matrix(ScalarKind kind, uint32_t columns, uint32_t rows) {
	const auto key = std::make_tuple(static_cast<uint32_t>(kind), rows, columns);
	const auto cached = _matrices.find(key);
	if (cached != _matrices.end()) {
		return cached->second;
	}

	const Id column = vector(kind, rows);

	// A module may declare an aggregate type only once, and OpTypeMatrix takes
	// the column type and the count of columns: the members are not listed, so a
	// count written as a trailing word would be read as one.
	_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Matrix) });
	const Id id = _builder.emitDecl(spirv::OpTypeMatrix, { column, columns });

	_matrices.emplace(key, id);
	_columnTypeOfMatrix.emplace(id, column);
	return id;
}

uint32_t TypeTable::matrixColumnCount(Id matrixType) const {
	for (const auto& [key, id]: _matrices) {
		if (id == matrixType) {
			return std::get<2>(key);
		}
	}

	return 0;
}

Id TypeTable::matrixColumnType(Id matrixType) const {
	const auto it = _columnTypeOfMatrix.find(matrixType);
	return it == _columnTypeOfMatrix.end() ? InvalidId : it->second;
}

spirv::Id TypeTable::blockStructFor(spirv::Id elementType) {
	const auto cached = _blockStructs.find(elementType);
	if (cached != _blockStructs.end()) {
		return cached->second;
	}

	// A runtime array has no length, which is what lets the descriptor cover a
	// Metal buffer whose size is not known when the shader is compiled.
	const Id runtimeArray = _builder.emitDecl(spirv::OpTypeRuntimeArray, { elementType });

	// The stride is how far apart two elements are, which for a scalar or a
	// vector is its own size and for a struct is the struct's. Metal rounds a
	// struct's size up to its alignment, and the largest alignment a member
	// can carry here is a vector's 16 bytes, so that is what an array of a
	// smaller struct steps by.
	uint32_t stride = 4;
	if (const auto it = _widthOfScalar.find(elementType); it != _widthOfScalar.end()) {
		stride = (it->second + 7) / 8;
	} else if (const auto it = _widthOfVector.find(elementType); it != _widthOfVector.end()) {
		uint32_t width = 32;
		// The vector's element width is not tracked separately, so this uses
		// the common cases; a wider vector is not in the corpus.
		for (const auto& [key, id]: _vectors) {
			if (id == elementType) {
				width = scalarBitWidth(static_cast<ScalarKind>(key.first));
			}
		}
		stride = ((width + 7) / 8) * it->second;
	} else if (const auto it = _structSizes.find(elementType); it != _structSizes.end()) {
		constexpr uint32_t kMaxAlignment = 16;
		stride = (it->second + kMaxAlignment - 1) / kMaxAlignment * kMaxAlignment;
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

ScalarKind TypeTable::imageComponent(spirv::Id imageType) const {
	for (const auto& [key, id]: _images) {
		if (id == imageType) {
			return static_cast<ScalarKind>(key.first);
		}
	}

	return ScalarKind::Void;
}

Id TypeTable::sampledImageOf(Id imageType) {
	const auto cached = _sampledImages.find(imageType);
	if (cached != _sampledImages.end()) {
		return cached->second;
	}

	const Id id = _builder.emitDecl(spirv::OpTypeSampledImage, { imageType });
	_sampledImages.emplace(imageType, id);
	return id;
}

Id TypeTable::functionType(Id returnType, const std::vector<Id>& parameterTypes) {
	// Keyed on the return type followed by the parameter types, which is the
	// order the operands are in.
	const std::vector<Id> key = [returnType, &parameterTypes]() {
		std::vector<Id> all = { returnType };
		all.insert(all.end(), parameterTypes.begin(), parameterTypes.end());
		return all;
	}();

	const auto cached = _functionTypes.find(key);
	if (cached != _functionTypes.end()) {
		return cached->second;
	}

	// No parameter count: like OpTypeStruct, the trailing count is derived from
	// the instruction's word count and is not in the binary. Writing one is read
	// as a parameter type, which shows up as "Id is 0".
	std::vector<uint32_t> operands(key.begin(), key.end());
	const Id id = _builder.emitDecl(spirv::OpTypeFunction, operands);

	_functionTypes.emplace(key, id);
	return id;
}

Id TypeTable::sampledImage(ScalarKind component, TextureDim dim) {
	const auto key = std::make_pair(static_cast<uint32_t>(component), static_cast<uint32_t>(dim));
	const auto cached = _images.find(key);
	if (cached != _images.end()) {
		return cached->second;
	}

	// OpTypeImage spells its middle operands as 0 or 1 rather than naming them,
	// and the grammar has no enumerants for them, so they are written out here:
	// Depth, Arrayed, Multisampled, then Sampled.
	constexpr uint32_t kNotDepth = 0;
	constexpr uint32_t kNotArrayed = 0;
	constexpr uint32_t kNotMultisampled = 0;
	constexpr uint32_t kSampled = 1;

	const uint32_t spirvDim = [&]() {
		switch (dim) {
			case TextureDim::D2: return static_cast<uint32_t>(spirv::Dim::Dim2D);
			case TextureDim::None: break;
		}

		throw CompileError("this texture shape has no SPIR-V mapping");
	}();

	// A Metal texture is declared as an image type, and the sampled-operand of
	// 1 is what makes it a sampled image: read through a sampler, with the
	// result being a texel value rather than the texel itself.
	const Id id = _builder.emitDecl(spirv::OpTypeImage, { scalar(component), spirvDim, kNotDepth,
		kNotArrayed, kNotMultisampled, kSampled,
		static_cast<uint32_t>(spirv::ImageFormat::Unknown) });

	_images.emplace(key, id);
	return id;
}

spirv::Id TypeTable::pointeeOf(spirv::Id pointerType) const {
	for (const auto& [key, id]: _pointers) {
		if (id == pointerType) {
			return key.second;
		}
	}

	return spirv::InvalidId;
}

ScalarKind TypeTable::componentKind(spirv::Id vectorType) const {
	for (const auto& [key, id]: _vectors) {
		if (id == vectorType) {
			return static_cast<ScalarKind>(key.first);
		}
	}

	return ScalarKind::Void;
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

uint32_t TypeTable::vectorWidth(spirv::Id type) const {
	auto it = _widthOfVector.find(type);
	if (it != _widthOfVector.end()) {
		return it->second;
	}

	return 1;
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

// A struct's member types, where each one starts in a buffer, and the size of
// the struct, which is how far an array of it steps. Metal lays a struct out
// with every member at the next multiple of its own size, so a float4 member
// both starts and steps 16 bytes, which is what a float2 field after it has to
// account for.
bool TypeTable::structMembersFor(const std::string& name, std::vector<Id>& outTypes,
	std::vector<uint32_t>& outOffsets, std::vector<uint32_t>* outStride, uint32_t* outSize) {

	const StructDecl* decl = _unit.findStruct(name);
	if (!decl) {
		return false;
	}

	uint32_t offset = 0;

	for (const StructField& field: decl->fields) {
		if (!field.type.isPointer && !field.type.namedType.empty()) {
			throw CompileError("struct \"" + decl->name + "\" has a field whose type is itself "
				"a struct, which mslc cannot represent yet (\"" + field.name + "\")");
		}

		Id fieldType;
		uint32_t size;
		// A matrix's rows are a vector's width, so the stride between its columns
		// is that vector's size, which is what SPIR-V's MatrixStride has to say.
		uint32_t stride = 0;
		if (field.type.isMatrix()) {
			fieldType = matrix(field.type.scalar, field.type.matrixColumns, field.type.matrixRows);
			stride = fieldTypeSize(field.type.scalar) * field.type.matrixRows;
			size = stride * field.type.matrixColumns;
		} else if (field.type.vectorWidth > 1) {
			fieldType = vector(field.type.scalar, field.type.vectorWidth);
			size = fieldTypeSize(field.type.scalar) * field.type.vectorWidth;
		} else {
			fieldType = scalar(field.type.scalar);
			size = fieldTypeSize(field.type.scalar);
		}

		if (size == 0) {
			throw CompileError("struct \"" + decl->name + "\" has a field mslc cannot represent "
				"yet (\"" + std::string(scalarKindName(field.type.scalar))
				+ (field.type.isPointer ? "*" : "") + " " + field.name + "\")");
		}

		outTypes.push_back(fieldType);
		outOffsets.push_back(offset);
		if (outStride) {
			outStride->push_back(stride);
		}
		offset += size;
	}

	if (outSize) {
		*outSize = offset;
	}

	return true;
}

uint32_t TypeTable::fieldTypeSize(ScalarKind kind) {
	const ScalarMapping mapping = mappingFor(kind);
	if (!mapping.supported) {
		throw CompileError(std::string("type ") + scalarKindName(kind) + " has no SPIR-V mapping");
	}

	return mapping.width / 8;
}

void TypeTable::decorateMatrixStride(Id structure, size_t member, uint32_t stride) {
	if (stride == 0) {
		return;
	}

	// ColMajor on the member, not on the struct: SPIR-V takes the row-major or
	// column-major of a matrix as a member decoration, and rejects the struct
	// form outright. Metal lays a matrix out in columns, which is also SPIR-V's
	// own default, so saying it explicitly is what satisfies Vulkan's demand
	// that a struct in a Block holding a matrix state its layout.
	_builder.emit(spirv::OpMemberDecorate, { structure, static_cast<uint32_t>(member),
		static_cast<uint32_t>(spirv::Decoration::ColMajor) });

	_builder.emit(spirv::OpMemberDecorate, { structure, static_cast<uint32_t>(member),
		static_cast<uint32_t>(spirv::Decoration::MatrixStride), stride });
}

Id TypeTable::namedStruct(const std::string& name) {
	const auto cached = _valueStructs.find(name);
	if (cached != _valueStructs.end()) {
		return cached->second;
	}

	std::vector<Id> fieldTypes;
	std::vector<uint32_t> offsets;
	if (!structMembersFor(name, fieldTypes, offsets, nullptr, nullptr)) {
		return InvalidId;
	}

	// Members only. The count is implied by the instruction's word count, and
	// writing it as an operand would be read as one extra member.
	std::vector<uint32_t> operands;
	for (Id fieldType: fieldTypes) {
		operands.push_back(fieldType);
	}

	// No Block and no member offsets: that is the layout of a buffer, and a
	// struct used as a value is laid out by the function it is declared in.
	const Id id = _builder.emitDecl(spirv::OpTypeStruct, operands);

	_valueStructs.emplace(name, id);
	return id;
}

Id TypeTable::arrayElementStruct(const std::string& name) {
	const auto cached = _elementStructs.find(name);
	if (cached != _elementStructs.end()) {
		return cached->second;
	}

	std::vector<Id> fieldTypes;
	std::vector<uint32_t> offsets;
	std::vector<uint32_t> strides;
	uint32_t size = 0;
	if (!structMembersFor(name, fieldTypes, offsets, &strides, &size)) {
		return InvalidId;
	}

	std::vector<uint32_t> operands;
	for (Id fieldType: fieldTypes) {
		operands.push_back(fieldType);
	}

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, operands);

	// Laid out, because Vulkan requires a struct nested inside a Block to be,
	// and not Block-decorated, because a Block inside an array is rejected.
	for (size_t i = 0; i < fieldTypes.size(); ++i) {
		_builder.emit(spirv::OpMemberDecorate, { id, static_cast<uint32_t>(i),
			static_cast<uint32_t>(spirv::Decoration::Offset), offsets[i] });
		decorateMatrixStride(id, i, strides[i]);
	}

	_elementStructs.emplace(name, id);
	_structSizes.emplace(id, size);
	return id;
}

Id TypeTable::blockStruct(const std::string& name) {
	const auto cached = _structs.find(name);
	if (cached != _structs.end()) {
		return cached->second;
	}

	std::vector<Id> fieldTypes;
	std::vector<uint32_t> offsets;
	std::vector<uint32_t> strides;
	if (!structMembersFor(name, fieldTypes, offsets, &strides, nullptr)) {
		return InvalidId;
	}

	std::vector<uint32_t> operands;
	for (Id fieldType: fieldTypes) {
		operands.push_back(fieldType);
	}

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, operands);

	// A struct reached through a buffer binding is an interface block, so it
	// needs Block and a per-member Offset, which is what tells the runtime
	// where each field sits in the buffer Metal laid out.
	_builder.emit(spirv::OpDecorate, { id, static_cast<uint32_t>(spirv::Decoration::Block) });
	for (size_t i = 0; i < fieldTypes.size(); ++i) {
		_builder.emit(spirv::OpMemberDecorate, { id, static_cast<uint32_t>(i),
			static_cast<uint32_t>(spirv::Decoration::Offset), offsets[i] });
		decorateMatrixStride(id, i, strides[i]);
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

	if (requested != Stage::None) {
		for (const FunctionDecl& function: unit.functions) {
			if (function.stage == requested) {
				found.push_back(&function);
			}
		}

		if (found.empty()) {
			throw CompileError(std::string("no ") + (requested == Stage::Kernel ? "kernel"
				: requested == Stage::Vertex ? "vertex" : "fragment")
				+ " entry point in this source");
		}

		return found;
	}

	// Declaration order, so a caller that asked for no stage gets the source's
	// own order back.
	for (const FunctionDecl& function: unit.functions) {
		if (function.stage != Stage::None) {
			found.push_back(&function);
		}
	}

	if (found.empty()) {
		throw CompileError("source declares no kernel, vertex or fragment entry point");
	}

	return found;
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
		spirv::StorageClassValue storageClass = spirv::StorageClass::Function;
		// For a struct-typed value, the declaration, so member access can
		// resolve a field.
		const StructDecl* structType = nullptr;
		// True when the MSL type is a scalar but the SPIR-V form is a vector,
		// so a use wants one component rather than the whole vector.
		bool scalarComponentOfVector = false;
		// True when the name is a descriptor over a wrapped buffer, so indexing
		// it needs a struct member index before the element index.
		bool isBuffer = false;
		// For a [[stage_in]] parameter, the variable of each member. SPIR-V
		// gives every member of an interface struct its own variable, since a
		// struct may not mix a builtin member with a located one, so there is no
		// struct to reach a member of.
		bool isStageIn = false;
		std::vector<Id> stageInMembers;
	};

	// A constant a file-scope "constant" declaration was folded to. SPIR-V takes
	// only constants as a constant's operands, so an initialiser that refers to
	// an earlier constant or does arithmetic has to be folded into a value here
	// rather than emitted as the expression it was written as.
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
		const std::vector<const FunctionDecl*>& _entryPoints;
		const ModuleOptions& _options;
		TypeTable& _types;

		// Per entry point: the names in scope are the entry point's own
		// parameters and locals, and its interface and its bindings are its own.
		std::map<std::string, Binding> _bindings;
		std::vector<Id> _interface;
		std::string _entryBindings;

		// Per module: a sampler declared in the shader, which any entry point may
		// use and so is not among an entry point's own names.
		std::map<std::string, Binding> _globalBindings;

		// Where the entry point's returned value goes, one entry per output
		// variable. A returned struct has one output per member, since SPIR-V
		// will not let a struct mix builtin and location members.
		struct Output {
			Id variable = InvalidId;
			Id type = InvalidId;
			// The member of the returned struct this output takes, or -1 when the
			// whole value is the output.
			int member = -1;
		};
		std::vector<Output> _outputs;

		// Whether the block being emitted already has a terminator. A block
		// ends at its terminator, so anything after one belongs to no block at
		// all until a label opens the next.
		bool _blockTerminated = false;

		// Per module: the scalar types and the GLSL set are named once however
		// many entry points refer to them.
		Id _uintType = InvalidId;
		Id _intType = InvalidId;
		Id _boolType = InvalidId;
		Id _voidType = InvalidId;
		bool _glslImported = false;
		spirv::Id _glslSet = InvalidId;

		// Per module: a sampler declared in the shader belongs to the module
		// rather than to any one entry point, since more than one can use it.
		std::string _moduleBindings;

		// Per module: what each file-scope constant folded to, so a later one can
		// refer to it by name and a use in a body can read it.
		std::map<std::string, FoldedConstant> _constants;

	public:
		Emitter(spirv::Builder& builder, const TranslationUnit& unit,
			const std::vector<const FunctionDecl*>& entryPoints, const ModuleOptions& options, TypeTable& types):
			_builder(builder), _unit(unit), _entryPoints(entryPoints),
			_options(options), _types(types) {}

		EmittedModule run();

	private:
		Id constantU32(uint32_t value);
		Id declaredTypeOf(const Type& type);
		void emitLabel(Id id);
		void emitTerminator(uint16_t opcode, std::vector<uint32_t> operands);
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
		void declareParameters(const FunctionDecl& entryPoint);
		std::vector<Output> declareOutputs(const FunctionDecl& entryPoint);
		EmittedEntryPoint emitEntryPoint(const FunctionDecl& entryPoint);
		void emitFunctionBody(const Statement& statement);
		void emitStatement(const Statement& statement);
		void emitVariableDeclaration(const VariableDeclaration& declaration);
		bool emitStore(const Expression& expression);

		// Every expression returns a value id whose type the builder knows.
		// For an lvalue such as "buffer[index]" the result is a pointer, and
		// the caller loads from it.
		Id emitExpression(const Expression& expression);
		Id emitBinary(const Expression& expression);
		Id emitMatrixProduct(Id left, Id right, Id leftType, Id rightType);
		Id emitUnary(const Expression& expression);
		Id emitIndex(const Expression& expression, bool asAddress);
		Id emitValueIndex(const Expression& expression);
		Id emitMember(const Expression& expression);
		Id emitMemberAddress(const Expression& expression);
		Id addressOf(const Expression& expression);
		const Binding& bindingFor(const Expression& expression);
		Id readSwizzle(Id vector, const std::string& name);
		std::vector<uint32_t> swizzleFor(Id vectorType, const std::string& name);
		void writeSwizzleComponent(Id object, const std::string& name, Id value);
		Id fieldOf(const StructDecl& decl, const std::string& name);
		bool isStructObject(const Expression& expression) const;
		const Binding* findBinding(const std::string& name) const;
		void spreadScalar(Id& value, Id& type, Id vectorType);
		Id spreadScalarTo(Id value, Id vectorType);
		Id emitCall(const Expression& expression);
		Id emitBuiltin(const Expression& expression);
		Id glslSet();
		Id floatConstant(double value);
		Id emitSample(const Expression& expression);
		Id emitCast(const Expression& expression);
		Id buildComposite(const Type& target, Id toType, const Expression& expression);
		Id emitIdentifier(const Expression& expression);
		Id loadFrom(Id pointer, Id pointeeType);
		Id convert(Id value, Id fromType, Id toType);
	};

	void Emitter::emitLabel(Id id) {
		// With the id the caller allocated rather than one the builder picks, so
		// a branch can be emitted naming a label that does not exist yet.
		_builder.emitDeclAt(spirv::OpLabel, id, { });
		_blockTerminated = false;
	}

	// A block ends at its terminator, which is tracked so a body that already
	// returned is not given a second, unreachable one after it.
	void Emitter::emitTerminator(uint16_t opcode, std::vector<uint32_t> operands) {
		_builder.emit(opcode, std::move(operands));
		_blockTerminated = true;
	}

	// No setSection here: OpConstant is routed to the types block by the
	// builder, and moving the current section would strand the caller's next
	// instruction in the wrong block.
	Id Emitter::constantU32(uint32_t value) {
		return _builder.emitDeclTyped(spirv::OpConstant, _uintType, { value });
	}

	Id Emitter::declaredTypeOf(const Type& type) {
		if (!type.namedType.empty()) {
			const Id id = _types.namedStruct(type.namedType);
			if (id == InvalidId) {
				throw CompileError("\"" + type.namedType + "\" is not a type this source declares");
			}

			return id;
		}

		if (type.isMatrix()) {
			return _types.matrix(type.scalar, type.matrixColumns, type.matrixRows);
		}

		if (type.vectorWidth > 1) {
			return _types.vector(type.scalar, type.vectorWidth);
		}

		return _types.scalar(type.scalar);
	}


	Id Emitter::loadFrom(Id pointer, Id pointeeType) {
		return _builder.emitTyped(spirv::OpLoad, pointeeType, { pointer });
	}

	Id Emitter::convert(Id value, Id fromType, Id toType) {
		if (fromType == toType) {
			return value;
		}

		// Integer and float conversions have their own opcodes; a bitcast covers
		// a reinterpretation of the same width, and is only a bitcast at that
		// width. Two floats of different widths are a narrowing, which is a
		// convert: "half4(aFloat4)" narrows each component rather than
		// reinterpreting four floats as two halves.
		const bool fromFloat = _types.isFloat(fromType);
		const bool toFloat = _types.isFloat(toType);

		if (fromFloat && !toFloat) {
			const bool toSigned = _types.isSignedInt(toType);
			return _builder.emitTyped(toSigned ? spirv::OpConvertFToS : spirv::OpConvertFToU, toType, { value });
		}

		if (!fromFloat && toFloat) {
			return _builder.emitTyped(spirv::OpConvertUToF, toType, { value });
		}

		// FConvert is scalar-only in SPIR-V, so a vector of one float width
		// becoming a vector of another is converted a component at a time and put
		// back together. The components keep their positions, which is what
		// "half4(aFloat4)" means: a narrowing of each one.
		if (fromFloat && toFloat) {
			if (_types.vectorWidth(toType) == 1) {
				return _builder.emitTyped(spirv::OpFConvert, toType, { value });
			}

			const Id fromScalar = _types.scalar(_types.componentKind(fromType));
			const Id toScalar = _types.scalar(_types.componentKind(toType));
			const uint32_t width = _types.vectorWidth(toType);

			std::vector<uint32_t> operands;
			for (uint32_t i = 0; i < width; ++i) {
				const Id part = _builder.emitTyped(spirv::OpCompositeExtract, fromScalar,
					{ value, i });
				operands.push_back(_builder.emitTyped(spirv::OpFConvert, toScalar, { part }));
			}

			return _builder.emitTyped(spirv::OpCompositeConstruct, toType, operands);
		}

		return _builder.emitTyped(spirv::OpBitcast, toType, { value });
	}

	Id Emitter::emitIdentifier(const Expression& expression) {
		// The entry point's own names, then the module's: a constant declared at
		// file scope is not a parameter of the entry point that reads it.
		const Binding* found = findBinding(expression.name);
		if (!found) {
			// A parameter is free to be named after an MSL builtin: Blender's
			// compute_buffer_clear names one "position". Reporting the collision
			// here rather than in the parser is what lets the declaration win.
			if (isMSLBuiltinName(expression.name)) {
				throw CompileError("builtin function \"" + expression.name
					+ "\" is not supported yet");
			}

			throw CompileError("\"" + expression.name + "\" is not a parameter, local or builtin "
				"mslc knows about");
		}

		const Binding& binding = *found;

		// A [[stage_in]] parameter is an interface, not a value: SPIR-V has no
		// struct holding the members, so there is nothing to load.
		if (binding.isStageIn) {
			throw CompileError("\"" + expression.name + "\" is a [[stage_in]] parameter, which "
				"is its members rather than a value; name one of them");
		}

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
		if (expression.arguments.size() != 1) {
			throw CompileError("expected exactly one index, found "
				+ std::to_string(expression.arguments.size()));
		}

		// A value rather than a place: "m[0]" where m is a matrix or a vector in
		// hand is a component or a column, and there is no address to chain to,
		// so the element is taken out of the value. This is what reaches a
		// matrix member of a buffer, which loads before it can be indexed.
		if (expression.left->kind != ExpressionKind::Identifier) {
			return emitValueIndex(expression);
		}

		const auto it = _bindings.find(expression.left->name);
		if (it == _bindings.end() || !it->second.isPointer) {
			throw CompileError("\"" + expression.left->name + "\" is not a pointer, so it cannot "
				"be indexed");
		}

		const Binding& binding = it->second;

		const Id index = emitExpression(*expression.arguments[0]);
		_builder.setType(index, _uintType);

		if (binding.isBuffer) {
			// The descriptor is a struct holding a runtime array, so reaching an
			// element means stepping into member 0 and then indexing the array.
			const Id address = _builder.emitTyped(spirv::OpAccessChain,
				_types.pointer(binding.storageClass, binding.pointeeType),
				{ binding.id, constantU32(0), index });

			return asAddress ? address : loadFrom(address, binding.pointeeType);
		}

		const Id address = _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(binding.storageClass, binding.pointeeType),
			{ binding.id, index });

		return asAddress ? address : loadFrom(address, binding.pointeeType);
	}

	// "object[index]" where the object is a value rather than a place: a column
	// of a matrix, or a component of a vector. There is no address to chain to
	// for either, since SPIR-V has no pointer to a matrix's column, so the
	// element is taken out of the value. This is what reaches a member of a
	// buffer, which has to be loaded before it can be indexed, and it is what
	// makes "m[0][1]" and "m[0].xy" work by indexing twice and swizzling once.
	Id Emitter::emitValueIndex(const Expression& expression) {
		const Id object = emitExpression(*expression.left);
		const Id objectType = _builder.typeOf(object);
		const Id column = _types.matrixColumnType(objectType);
		const uint32_t width = _types.vectorWidth(objectType);

		if (column == InvalidId && width < 2) {
			throw CompileError("this expression is not a matrix or a vector, and neither has "
				"anything to index");
		}

		const Expression& index = *expression.arguments[0];
		const uint32_t count = column == InvalidId ? width : _types.matrixColumnCount(objectType);
		const Id element = column == InvalidId
			? _types.scalar(_types.componentKind(objectType))
			: column;

		// A literal index goes into the instruction as a literal, not as the id of
		// a constant, which is the whole difference between the static and the
		// dynamic form of an extract.
		if (index.kind == ExpressionKind::IntLiteral) {
			if (index.intValue >= count) {
				throw CompileError("an index of " + std::to_string(index.intValue)
					+ " is past the end of " + std::to_string(count)
					+ (column == InvalidId ? " components" : " columns"));
			}

			return _builder.emitTyped(spirv::OpCompositeExtract, element,
				{ object, static_cast<uint32_t>(index.intValue) });
		}

		if (column != InvalidId) {
			throw CompileError("only a literal index reaches a matrix's column, since SPIR-V has "
				"no instruction for a column at an index the shader computes");
		}

		// A component of a vector at a computed index, which SPIR-V does have an
		// instruction for. The index is a value here rather than a literal.
		Id at = emitExpression(index);
		_builder.setType(at, _uintType);

		return _builder.emitTyped(spirv::OpVectorExtractDynamic, element, { object, at });
	}

	// A write through a place. An assignment yields an address rather than a
	// value, so it never goes through emitExpression, and a for loop's increment
	// is a statement like any other, so it reaches this too rather than the
	// expression path. Returns false for anything that is not an assignment.
	bool Emitter::emitStore(const Expression& expression) {
		if (expression.kind != ExpressionKind::Assign) {
			return false;
		}

		// A component of a vector has no address of its own, so a write to one
		// replaces the component in the vector rather than storing through a
		// pointer.
		if (expression.left->kind == ExpressionKind::Member
			&& !isStructObject(*expression.left->left)
			&& swizzleIndices(expression.left->memberName)) {
			const Id object = addressOf(*expression.left->left);
			writeSwizzleComponent(object, expression.left->memberName,
				emitExpression(*expression.right));
			return true;
		}

		// The target is a place, so it is emitted as an address rather than
		// loaded. addressOf is what says what a place is: an index chains through
		// the buffer, a member through the struct, and a name is its own variable.
		const Id address = addressOf(*expression.left);
		const Id value = emitExpression(*expression.right);

		// A store's value has the type the address points at, not the type of the
		// address, so the pointee is what the value is converted to.
		const Id pointeeType = _types.pointeeOf(_builder.typeOf(address));
		if (pointeeType == spirv::InvalidId) {
			throw CompileError("cannot determine what this assignment writes through, "
				"so the store cannot be typed");
		}

		_builder.emit(spirv::OpStore, { address,
			convert(value, _builder.typeOf(value), pointeeType) });
		return true;
	}

	Id Emitter::fieldOf(const StructDecl& decl, const std::string& name) {
		for (size_t i = 0; i < decl.fields.size(); ++i) {
			if (decl.fields[i].name == name) {
				return static_cast<Id>(i);
			}
		}

		throw CompileError("struct \"" + decl.name + "\" has no member \"" + name + "\"");
	}

	// The binding an lvalue starts from. A member chain stays in the storage
	// class of the variable or parameter it is rooted at, so an access chain
	// along it is typed with that one's storage class. An index is part of the
	// chain rather than the end of it, since a buffer element's member is
	// reached as "buffer[index].field".
	const Binding& Emitter::bindingFor(const Expression& expression) {
		const Expression* current = &expression;
		while (current->kind == ExpressionKind::Member || current->kind == ExpressionKind::Index) {
			current = current->left.get();
		}

		if (current->kind != ExpressionKind::Identifier) {
			throw CompileError("this expression is not rooted at a name, so it has no storage");
		}

		if (const Binding* binding = findBinding(current->name)) {
			return *binding;
		}

		throw CompileError("\"" + current->name + "\" is not a parameter or local");
	}

	// An entry point's own names, then the module's: a sampler declared in the
	// shader is not a parameter of the entry point that reads it.
	const Binding* Emitter::findBinding(const std::string& name) const {
		const auto own = _bindings.find(name);
		if (own != _bindings.end()) {
			return &own->second;
		}

		const auto global = _globalBindings.find(name);
		return global == _globalBindings.end() ? nullptr : &global->second;
	}

	// The address a name or a member chain denotes, for a store to write through.
	Id Emitter::addressOf(const Expression& expression) {
		if (expression.kind == ExpressionKind::Member) {
			return emitMemberAddress(expression);
		}

		// An element of a pointer, which is how a buffer element's member is
		// reached: "vertices[vid].color".
		if (expression.kind == ExpressionKind::Index) {
			return emitIndex(expression, true);
		}

		if (expression.kind != ExpressionKind::Identifier) {
			throw CompileError("this expression cannot be assigned to");
		}

		const Binding& binding = bindingFor(expression);
		if (!binding.isPointer) {
			throw CompileError("\"" + expression.name + "\" is not a place, so it cannot "
				"be assigned to");
		}

		// A buffer of values is a wrapper struct holding a runtime array, so the
		// first element is two steps in: into the member, then into the array at
		// index zero. Both indices are constants, since a struct may only be
		// indexed by one.
		if (binding.isBuffer) {
			return _builder.emitTyped(spirv::OpAccessChain,
				_types.pointer(binding.storageClass, binding.pointeeType),
				{ binding.id, constantU32(0), constantU32(0) });
		}

		return binding.id;
	}

	Id Emitter::emitMemberAddress(const Expression& expression) {
		const Binding& binding = bindingFor(expression);
		if (!binding.structType) {
			throw CompileError("\"" + expression.left->name + "\" is not a struct, so \"."
				+ expression.memberName + "\" is not a member of it");
		}

		const Id fieldIndex = fieldOf(*binding.structType, expression.memberName);
		const Id fieldType = declaredTypeOf(binding.structType->fields[fieldIndex].type);

		// Each member of a [[stage_in]] struct is its own variable, so a member
		// of one is that variable rather than anything reached from it.
		if (binding.isStageIn) {
			return binding.stageInMembers[fieldIndex];
		}

		// The result type is a pointer to the field, not to the struct the chain
		// starts from. A chain typed as a pointer to the struct does not match
		// what the base indexes to, and spirv-val rejects it.
		return _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(binding.storageClass, fieldType),
			{ addressOf(*expression.left), constantU32(fieldIndex) });
	}

	// A swizzle naming a component the vector does not have is a mistake worth
	// naming, rather than an out-of-range index for the validator to find. Given
	// the type rather than a value, so that checking a name costs no load.
	std::vector<uint32_t> Emitter::swizzleFor(Id vectorType, const std::string& name) {
		const std::vector<uint32_t> indices = *swizzleIndices(name);
		const uint32_t width = _types.vectorWidth(vectorType);

		for (const uint32_t index: indices) {
			if (index >= width) {
				throw CompileError("a vector of " + std::to_string(width)
					+ " components has no component \"" + name + "\"");
			}
		}

		return indices;
	}

	// A component or a set of components of a vector, read as a value.
	Id Emitter::readSwizzle(Id vector, const std::string& name) {
		const Id type = _builder.typeOf(vector);
		const std::vector<uint32_t> indices = swizzleFor(type, name);
		const ScalarKind component = _types.componentKind(type);

		// One component is an extract; several are a shuffle, which takes the
		// vector twice, once for the components and once for the result.
		if (indices.size() == 1) {
			return _builder.emitTyped(spirv::OpCompositeExtract, _types.scalar(component),
				{ vector, indices.front() });
		}

		std::vector<uint32_t> operands = { vector, vector };
		for (const uint32_t index: indices) {
			operands.push_back(index);
		}

		return _builder.emitTyped(spirv::OpVectorShuffle,
			_types.vector(component, static_cast<uint32_t>(indices.size())), operands);
	}

	// A write to one component of a vector. A component has no address of its
	// own to chain to, so the vector is read, the component replaced in it and
	// the result written back over the whole thing. Given the vector's address,
	// which the caller has already emitted.
	void Emitter::writeSwizzleComponent(Id object, const std::string& name, Id value) {
		const Id type = _types.pointeeOf(_builder.typeOf(object));
		const std::vector<uint32_t> indices = swizzleFor(type, name);

		if (indices.size() != 1) {
			throw CompileError("writing \"" + name + "\" would write "
				+ std::to_string(indices.size()) + " components at once, which mslc does not do; "
				"assign each component on its own");
		}

		const Id loaded = loadFrom(object, type);
		// Operand order is the vector, then the component, then its index.
		const Id inserted = _builder.emitTyped(spirv::OpVectorInsertDynamic, type,
			{ loaded, value, constantU32(indices.front()) });

		_builder.emit(spirv::OpStore, { object,
			convert(inserted, _builder.typeOf(inserted), type) });
	}

	// A struct's fields are named whatever the source called them, and the
	// component letters are ordinary enough as field names that "p" is both a
	// component and a plausible field, so an object that is a struct is a field
	// access whatever the name is.
	bool Emitter::isStructObject(const Expression& expression) const {
		if (expression.kind != ExpressionKind::Identifier) {
			return false;
		}

		const Binding* binding = findBinding(expression.name);
		return binding && binding->structType != nullptr;
	}

	Id Emitter::emitMember(const Expression& expression) {
		// A component or a set of components of a vector is read out of the
		// value it is applied to; anything else names a field of a struct and is
		// read through its address.
		if (!isStructObject(*expression.left) && swizzleIndices(expression.memberName)) {
			const Id object = emitExpression(*expression.left);
			if (_types.componentKind(_builder.typeOf(object)) != ScalarKind::Void) {
				return readSwizzle(object, expression.memberName);
			}
		}

		// As with indexing, a member read yields the field and a member write
		// needs its address, so the address form does the work and this loads.
		const Binding& binding = bindingFor(expression);
		if (!binding.structType) {
			throw CompileError("\"" + expression.left->name + "\" is not a struct, so \"."
				+ expression.memberName + "\" is not a member of it");
		}

		const Id fieldIndex = fieldOf(*binding.structType, expression.memberName);
		const Id fieldType = declaredTypeOf(binding.structType->fields[fieldIndex].type);

		// A module-scope constant is a value rather than a place, so there is no
		// address to load through and the field is taken from the value. Every
		// other struct-valued name is a place, a [[stage_in]] parameter included:
		// its member is an interface variable of its own.
		if (!binding.isPointer && !binding.isStageIn) {
			return _builder.emitTyped(spirv::OpCompositeExtract, fieldType,
				{ emitExpression(*expression.left), fieldIndex });
		}

		return loadFrom(emitMemberAddress(expression), fieldType);
	}

	Id Emitter::emitCast(const Expression& expression) {
		const Type& target = *expression.castType;
		const Id toType = declaredTypeOf(target);

		// Metal spells a cast and a constructor the same way, so the argument
		// list is what tells them apart: one argument converts, several build.
		if (expression.arguments.size() != 1) {
			return buildComposite(target, toType, expression);
		}

		const Id value = emitExpression(*expression.arguments[0]);
		const Id fromType = _builder.typeOf(value);

		// A scalar spread across a vector, which is what "float4(0.5)" means:
		// every component takes the value. Metal spells the same thing with a
		// constructor rather than a cast, so this is reached from both.
		//
		// A vector into a vector is not a spread at all: "half4(aFloat4)" keeps
		// each component where it is and converts it, so it goes down the ordinary
		// conversion path and only a scalar is spread.
		if (target.vectorWidth > 1 && _types.vectorWidth(fromType) == 1) {
			// The value is converted to the component type first, which is what
			// makes "float3(0)" a splat of zero rather than a mistake: the integer
			// literal widens to a float, as it does for a scalar.
			const Id component = _types.scalar(_types.componentKind(toType));
			return spreadScalarTo(convert(value, fromType, component), toType);
		}

		if (target.isMatrix()) {
			throw CompileError("a matrix is built from one value per column, so a single value "
				"cannot be spread across its columns the way one is across a vector's");
		}

		return convert(value, fromType, toType);
	}

	// A vector or a matrix built from a list of values: "float4(1, 2, 3, 4)" and
	// "float4x4(c0, c1, c2, c3)". The values are positional and each is one
	// element, which is a scalar for a vector and a whole column for a matrix.
	Id Emitter::buildComposite(const Type& target, Id toType, const Expression& expression) {
		const bool isMatrix = target.isMatrix();
		const uint32_t expected = isMatrix ? target.matrixColumns : target.vectorWidth;
		const Id element = isMatrix
			? _types.vector(target.scalar, target.matrixRows)
			: _types.scalar(target.scalar);

		// A vector leaves room for one more value rather than being all of them
		// at once, which is how "float4(aFloat3, 1.0)" fills the fourth
		// component. The first argument's own width says whether that is what
		// was written, so a three-wide vector and a scalar is one form rather
		// than two, and a count alone does not have to be guessed at.
		if (!isMatrix && expression.arguments.size() == 2) {
			const Id first = emitExpression(*expression.arguments[0]);
			const Id firstType = _builder.typeOf(first);
			if (_types.vectorWidth(firstType) == expected - 1) {
				const Id last = emitExpression(*expression.arguments[1]);
				const Id lastType = _builder.typeOf(last);

				// The wide argument converts to the component type at its own
				// width, since that is the vector it becomes: "half4(aFloat3, 1)"
				// narrows the three and then widens one scalar beside them.
				return _builder.emitTyped(spirv::OpCompositeConstruct, toType,
					{ convert(first, firstType, _types.vector(target.scalar, expected - 1)),
					  convert(last, lastType, element) });
			}
		}

		if (expression.arguments.size() != expected) {
			throw CompileError(std::string(scalarKindName(target.scalar))
				+ (isMatrix ? std::to_string(expected) + "x" + std::to_string(target.matrixRows)
					: std::to_string(expected))
				+ " takes " + std::to_string(expected)
				+ (isMatrix ? " columns" : " values") + ", found "
				+ std::to_string(expression.arguments.size()));
		}

		std::vector<uint32_t> operands;
		for (const ExpressionPtr& argument: expression.arguments) {
			const Id part = emitExpression(*argument);
			operands.push_back(convert(part, _builder.typeOf(part), element));
		}

		return _builder.emitTyped(spirv::OpCompositeConstruct, toType, operands);
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

	// A scalar spread across a vector, which is what a mixed scalar and vector
	// operation needs: SPIR-V's binary arithmetic has no mixed form, so both
	// sides have to be the same type. This is what makes "v * 0.5" and
	// "v + 0.5" mean in Metal what they mean here.
	//
	// The usual arithmetic conversions come first, and they are the same rule the
	// two-scalar case follows: a scalar of another type becomes one of the vector's
	// own component type, so "v * 2" is a float4 times a float4 rather than a
	// shape mismatch.
	void Emitter::spreadScalar(Id& value, Id& type, Id vectorType) {
		const Id component = _types.scalar(_types.componentKind(vectorType));
		if (type != component) {
			if (_types.vectorWidth(type) > 1 || _types.isFloat(type) == _types.isFloat(component)) {
				throw CompileError("this operator mixes a vector of "
					+ std::to_string(_types.vectorWidth(vectorType)) + " with something that is not "
					"one of its own components, or is of a type it does not convert to, which "
					"mslc does not do");
			}

			value = convert(value, type, component);
			type = component;
		}

		value = spreadScalarTo(value, vectorType);
		type = vectorType;
	}

	// The same spread as a value, for the callers that build a vector rather
	// than widen an operand.
	Id Emitter::spreadScalarTo(Id value, Id vectorType) {
		return _builder.emitTyped(spirv::OpCompositeConstruct, vectorType,
			std::vector<uint32_t>(_types.vectorWidth(vectorType), value));
	}

	// A product with a matrix operand, which SPIR-V spells with an instruction
	// per shape of the other operand rather than with one binary multiply: a
	// matrix times a vector, a vector times a matrix and a matrix times a
	// matrix are three different products, not one commutative operation. The
	// operand order picks which, so it is read rather than normalised: "v * m"
	// is not "m * v".
	//
	// The result of a product with a vector is the matrix's column type, since
	// a column is one component of the result. The arithmetic itself is left to
	// the instruction, which reads the matrix in the column-major order Metal
	// wrote it in, so the only way to get this wrong here is to pick the
	// instruction whose operand order does not match the source.
	Id Emitter::emitMatrixProduct(Id left, Id right, Id leftType, Id rightType) {
		const Id leftColumn = _types.matrixColumnType(leftType);
		const Id rightColumn = _types.matrixColumnType(rightType);

		if (leftColumn != InvalidId && rightColumn != InvalidId) {
			if (leftType != rightType) {
				throw CompileError("a product of two matrices has to be of the same shape, found "
					+ std::to_string(_types.matrixColumnCount(leftType)) + " columns of "
					+ std::to_string(_types.vectorWidth(leftColumn)) + " and "
					+ std::to_string(_types.matrixColumnCount(rightType)) + " columns of "
					+ std::to_string(_types.vectorWidth(rightColumn)));
			}

			return _builder.emitTyped(spirv::OpMatrixTimesMatrix, leftType, { left, right });
		}

		if (leftColumn != InvalidId) {
			if (rightType != leftColumn) {
				throw CompileError("a matrix of " + std::to_string(_types.vectorWidth(leftColumn))
					+ " rows times a vector of " + std::to_string(_types.vectorWidth(rightType))
					+ " components is a shape mismatch");
			}

			return _builder.emitTyped(spirv::OpMatrixTimesVector, leftColumn, { left, right });
		}

		// A vector on the left, so the matrix is the second operand: SPIR-V
		// spells this with its own instruction rather than by reversing the
		// operands of the matrix-on-the-left form, because the two products are
		// not the same product.
		if (leftType != rightColumn) {
			throw CompileError("a vector of " + std::to_string(_types.vectorWidth(leftType))
				+ " components times a matrix of " + std::to_string(_types.vectorWidth(rightColumn))
				+ " rows is a shape mismatch");
		}

		return _builder.emitTyped(spirv::OpVectorTimesMatrix, leftType, { left, right });
	}

	Id Emitter::emitBinary(const Expression& expression) {
		Id left = emitExpression(*expression.left);
		Id right = emitExpression(*expression.right);
		Id leftType = _builder.typeOf(left);
		Id rightType = _builder.typeOf(right);

		// A matrix is not a vector of its own component type, so the mixed-form
		// path below would try to spread one across the other. It has its own
		// instructions instead.
		if (_types.matrixColumnType(leftType) != InvalidId
			|| _types.matrixColumnType(rightType) != InvalidId) {

			if (expression.binaryOperator != BinaryOperator::Multiply) {
				throw CompileError("a matrix operand of a binary operator is recognised only for "
					"the product, which is the only form with an instruction for it");
			}

			return emitMatrixProduct(left, right, leftType, rightType);
		}

		// Two operand types that differ have to become one before the operation has
		// an instruction: SPIR-V's binary arithmetic is typed on both sides being
		// the same type and converts neither. A vector and a scalar is a spread,
		// which is the mixed form it does have. Two scalars of different types is
		// what MSL's usual arithmetic conversions resolve instead, and the float
		// wins: a float compared against an integer literal is a floating-point
		// comparison against a widened zero, not an integer one, which is what
		// "intensity > 0" means.
		if (leftType != rightType) {
			const uint32_t leftWidth = _types.vectorWidth(leftType);
			const uint32_t rightWidth = _types.vectorWidth(rightType);

			if (leftWidth > 1 || rightWidth > 1) {
				if (leftWidth > 1) {
					spreadScalar(right, rightType, leftType);
				} else {
					spreadScalar(left, leftType, rightType);
					leftType = rightType;
				}
			} else if (_types.isFloat(leftType) != _types.isFloat(rightType)) {
				if (_types.isFloat(leftType)) {
					right = convert(right, rightType, leftType);
				} else {
					left = convert(left, leftType, rightType);
					leftType = rightType;
				}
			} else {
				throw CompileError("this operator's two operands are of different types, "
					"which mslc does not resolve to a common one");
			}
		}

		const bool isFloat = _types.isFloat(leftType);
		const bool isSigned = _types.isSignedInt(leftType);
		const bool isLogical = expression.binaryOperator == BinaryOperator::LogicalAnd
			|| expression.binaryOperator == BinaryOperator::LogicalOr;

		// A comparison or logical operator yields a bool regardless of operand
		// type; an arithmetic one yields its operand type.
		const bool isComparison = !isLogical && leftType != _boolType
			&& isComparisonOperator(expression.binaryOperator);

		// Neither yields a scalar bool, so a vector operand would need a
		// comparison per component, which this does not lower.
		if ((isLogical || isComparison) && _types.vectorWidth(leftType) > 1) {
			throw CompileError("a vector operand of a comparison or logical operator is not "
				"lowered yet");
		}

		if (isLogical || isComparison) {
			const uint16_t opcode = isLogical
				? (expression.binaryOperator == BinaryOperator::LogicalAnd
					? spirv::OpLogicalAnd : spirv::OpLogicalOr)
				: comparisonOpcode(expression.binaryOperator, isFloat, isSigned);

			return _builder.emitTyped(opcode, _boolType, { left, right });
		}

		return _builder.emitTyped(arithmeticOpcode(expression.binaryOperator, isFloat),
			leftType, { left, right });
	}

	Id Emitter::emitCall(const Expression& expression) {
		// A texture read. Metal writes it as a method on the texture taking the
		// sampler as an argument; SPIR-V takes the image and the sampler
		// separately and combines them into something a read can be made
		// through.
		if (expression.left->kind == ExpressionKind::Member
			&& expression.left->memberName == "sample") {
			return emitSample(expression);
		}

		if (expression.left->kind != ExpressionKind::Identifier) {
			throw CompileError("only a direct function call is supported");
		}

		return emitBuiltin(expression);
	}

	// The GLSL.std.450 set, imported once however many builtins a source calls,
	// since a module may declare it only once. The set's name is a literal operand
	// of the import rather than the id of an OpString, so it is packed straight
	// into the instruction.
	Id Emitter::glslSet() {
		if (!_glslImported) {
			_glslImported = true;

			std::vector<uint32_t> operands;
			spirv::Builder::appendString(operands, "GLSL.std.450");
			_glslSet = _builder.emitDecl(spirv::OpExtInstImport, operands);
		}

		return _glslSet;
	}

	Id Emitter::floatConstant(double value) {
		return _builder.emitDeclTyped(spirv::OpConstant,
			_types.scalar(ScalarKind::Float), { constantBits(ScalarKind::Float, value) });
	}

	Id Emitter::emitBuiltin(const Expression& expression) {
		const std::string& name = expression.left->name;
		const std::vector<ExpressionPtr>& arguments = expression.arguments;

		const auto arity = [&](size_t expected) {
			if (arguments.size() != expected) {
				throw CompileError(name + " takes " + std::to_string(expected)
					+ (expected == 1 ? " argument" : " arguments") + ", found "
					+ std::to_string(arguments.size()));
			}
		};

		// dot is a core instruction rather than an extended one, because
		// GLSL.std.450 has no Dot. Its result is a scalar of the operands'
		// component type, which is the one thing about it the first operand's type
		// does not already say.
		if (name == "dot") {
			arity(2);

			const Id left = emitExpression(*arguments[0]);
			const Id right = emitExpression(*arguments[1]);
			const Id type = _builder.typeOf(left);

			if (type != _builder.typeOf(right) || _types.vectorWidth(type) < 2) {
				throw CompileError("dot takes two vectors of the same type, and the arguments "
					"are not that");
			}

			return _builder.emitTyped(spirv::OpDot,
				_types.scalar(_types.componentKind(type)), { left, right });
		}

		// Metal's saturate is a clamp between zero and one, so the bounds are not
		// in the source at all: FClamp with the operand's own zero and one. The
		// bounds take the operand's shape, since FClamp requires all three of its
		// operands to have the result type, and a scalar is already that shape. Only
		// the float form is lowered: GLSL.std.450 has a separate instruction per
		// element type, and using the float one on an integer would be a module
		// that validates and clamps wrongly.
		if (name == "saturate") {
			arity(1);

			const Id value = emitExpression(*arguments[0]);
			const Id type = _builder.typeOf(value);

			if (!_types.isFloat(type)) {
				throw CompileError("saturate of a non-float is not lowered yet");
			}

			const Id zero = floatConstant(0.0);
			const Id one = floatConstant(1.0);
			const Id low = _types.vectorWidth(type) == 1 ? zero : spreadScalarTo(zero, type);
			const Id high = _types.vectorWidth(type) == 1 ? one : spreadScalarTo(one, type);

			// The instruction set and the instruction come before the arguments,
			// which is the order OpExtInst takes them in.
			return _builder.emitTyped(spirv::OpExtInst, type,
				{ glslSet(), static_cast<uint32_t>(spirv::glsl450::FClamp), value, low, high });
		}

		const std::optional<BuiltinMath> builtin = builtinMathFor(name);
		if (!builtin) {
			throw CompileError("function \"" + name + "\" is not a builtin mslc recognises, "
				"and user functions are not lowered yet");
		}

		arity(builtin->arguments);

		// Operand order: the instruction set, the instruction, then the source's
		// arguments in the order they were written, which is the order GLSL.std.450
		// takes them in. The result has the first argument's type, which is what
		// each of these returns, so that one is emitted first and its type kept.
		const Id first = emitExpression(*arguments[0]);
		std::vector<uint32_t> operands = { glslSet(),
			static_cast<uint32_t>(builtin->instruction), first };
		for (size_t i = 1; i < arguments.size(); ++i) {
			operands.push_back(emitExpression(*arguments[i]));
		}

		return _builder.emitTyped(spirv::OpExtInst, _builder.typeOf(first), operands);
	}

	Id Emitter::emitSample(const Expression& expression) {
		if (expression.arguments.size() != 2) {
			throw CompileError("sample takes a sampler and a coordinate, found "
				+ std::to_string(expression.arguments.size()) + " arguments");
		}

		const Binding& texture = bindingFor(*expression.left->left);
		const ScalarKind component = _types.imageComponent(texture.pointeeType);
		if (component == ScalarKind::Void) {
			throw CompileError("\"sample\" is a method on a texture, and what it is called on "
				"is not one");
		}

		const Binding& sampler = bindingFor(*expression.arguments[0]);
		if (sampler.pointeeType != _types.sampler()) {
			throw CompileError("sample's first argument has to be a sampler, and what was given "
				"is not one");
		}

		const Id combined = _builder.emitTyped(spirv::OpSampledImage,
			_types.sampledImageOf(texture.pointeeType),
			{ loadFrom(texture.id, texture.pointeeType), loadFrom(sampler.id, sampler.pointeeType) });

		// A read of a sampled texture is implicit-LOD, since the stage before
		// chose the level.
		const Id coordinate = emitExpression(*expression.arguments[1]);

		return _builder.emitTyped(spirv::OpImageSampleImplicitLod,
			_types.vector(component, 4), { combined, coordinate });
	}

	Id Emitter::emitExpression(const Expression& expression) {
		switch (expression.kind) {
			case ExpressionKind::IntLiteral: {
				return _builder.emitDeclTyped(spirv::OpConstant, _uintType,
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
			case ExpressionKind::Cast: return emitCast(expression);
			case ExpressionKind::InitList:
			case ExpressionKind::Assign: break;
		}

		// An assignment is a statement, so reaching it here means it was used
		// as a value, which MSL does not allow.
		throw CompileError("an assignment cannot be used as a value");
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

		const uint32_t bits = constantBits(folded.scalar, folded.number);
		return _builder.emitDeclTyped(spirv::OpConstant,
			_types.scalar(folded.scalar), { bits });
	}

	// A struct's fields are initialised by name in Metal, and every one of them has
	// to be given, so the list is checked against the declaration field by field
	// rather than taken positionally.
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
				const auto found = _constants.find(expression.name);
				if (found == _constants.end()) {
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
			case BinaryOperator::Modulo: value = l % r; break;
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

	// A file-scope "constant" is a compile-time constant, so its initialiser is
	// folded and the result declared as one of the module's constants. The folded
	// value is kept as well as the id, since a later constant referring to this
	// one needs the value rather than a reference to a constant.
	Id Emitter::declareGlobalConstant(const VariableDeclaration& declaration) {
		if (!declaration.initializer) {
			throw CompileError("the constant \"" + declaration.name
				+ "\" was declared without an initialiser");
		}

		const FoldedConstant folded = foldInitializer(declaration.type, *declaration.initializer);
		const Id id = emitConstant(folded);
		_constants[declaration.name] = folded;
		return id;
	}

	// The bits of a scalar constant of the given kind. A float is narrowed to its
	// own width here, since the value was folded as a double and OpConstant takes
	// the bits of the type it declares.
	uint32_t Emitter::constantBits(ScalarKind kind, double value) {
		// A 64-bit constant's literal is two words, which mslc does not write, so
		// one of those is reported rather than emitted with half of it.
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

			// The declared type is what the constant is. An integer literal may
			// initialise a float, since that is a widening; the other direction
			// would have to round, which is not something to do silently.
			if (folded.scalar == ScalarKind::Bool) {
				throw CompileError("a bool constant is initialised with true or false, found "
					+ std::string(scalarKindName(folded.scalar)));
			}

			if (isFloatKind(folded.scalar) && !isFloatKind(type.scalar)) {
				throw CompileError("a float cannot initialise a "
					+ std::string(scalarKindName(type.scalar)));
			}

			if (type.scalar == ScalarKind::Bool) {
				throw CompileError("a bool constant is initialised with true or false");
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

	// MSL does not require [[buffer(n)]] on an entry point's device or
	// constant parameters: an unbinding parameter's index is its position
	// among the binding parameters, with builtins skipped. add.metal relies on
	// this, since none of its pointers carry an attribute.
	std::vector<const Parameter*> assignImplicitBindings(const FunctionDecl& entryPoint) {
		std::vector<const Parameter*> bound;

		for (const Parameter& parameter: entryPoint.parameters) {
			// A stage interface is not a resource, so it takes no binding index
			// of its own any more than a builtin does.
			if (parameter.attributes.bufferIndex || parameter.attributes.textureIndex
				|| parameter.attributes.samplerIndex || parameter.attributes.builtin
				|| parameter.attributes.stageIn) {
				continue;
			}

			bound.push_back(&parameter);
		}

		return bound;
	}

	// A sampler declared in the shader has no Metal argument index, since it is
	// not a parameter of anything, so it shares no index with the resources that
	// are. Set 0 is what an entry point's own bindings use.
	constexpr uint32_t kSamplerSet = 1;

	// A sampler *parameter* does have a Metal index, and it is the index of the
	// texture it reads: [[texture(0)]] and [[sampler(0)]] are both 0. A Vulkan
	// descriptor set has one binding per index, so the two cannot share one, and
	// spirv-val does not notice two variables claiming the same set and binding,
	// so a collision here would be a module that validates and a pipeline that
	// does not behave. A third set separates them. Each kind of resource is
	// numbered by its own Metal index within its own set, and no two kinds can
	// land on the same one.
	constexpr uint32_t kSamplerParameterSet = 2;

	// A sampler state declared at file scope. Metal has no way to write a
	// sampler as a constant in a shader, so the descriptor stands in for the
	// state and the state itself has to reach the runtime: this reports the
	// state a default-constructed MSL sampler has, which is what the parser
	// accepts today. Everything else a VkSampler needs is either Vulkan's own
	// default or unreachable under clamp-to-edge, which is the address mode
	// reported here, so the border colour is the only one left out.
	void Emitter::declareGlobals() {
		uint32_t samplers = 0;

		for (const VariableDeclaration& global: _unit.globals) {
			if (!global.type.isSampler) {
				// A file-scope "constant" is a compile-time constant, not a
				// descriptor, so it is declared among the module's constants with
				// no binding of its own.
				_builder.setSection(spirv::Section::TypesGlobals);
				const Id id = declareGlobalConstant(global);

				// A value rather than a place, so a use of the name is the
				// constant itself and its members are extracted from it.
				Binding binding;
				binding.id = id;
				binding.pointeeType = id;
				if (!global.type.namedType.empty()) {
					binding.structType = _unit.findStruct(global.type.namedType);
				}
				_globalBindings[global.name] = binding;
				continue;
			}

			const size_t index = samplers++;

			// Before the type is asked for, since a type declaration is emitted
			// into whichever section is current.
			_builder.setSection(spirv::Section::TypesGlobals);
			const Id samplerType = _types.sampler();

			const Id pointerType = _types.pointer(spirv::StorageClass::UniformConstant, samplerType);
			const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
				{ static_cast<uint32_t>(spirv::StorageClass::UniformConstant) });

			_builder.setSection(spirv::Section::Annotations);
			_builder.emit(spirv::OpDecorate, { id,
				static_cast<uint32_t>(spirv::Decoration::DescriptorSet), kSamplerSet });
			_builder.emit(spirv::OpDecorate, { id,
				static_cast<uint32_t>(spirv::Decoration::Binding),
				static_cast<uint32_t>(index) });

			Binding binding;
			binding.id = id;
			binding.isPointer = true;
			binding.pointeeType = samplerType;
			binding.storageClass = spirv::StorageClass::UniformConstant;
			_globalBindings[global.name] = binding;

			_moduleBindings += "\t\t{ \"kind\": \"Sampler\", \"descriptor\": { \"set\": "
				+ std::to_string(kSamplerSet) + ", \"binding\": " + std::to_string(index) + " }"
				+ ", \"name\": \"" + global.name + "\""
				+ ", \"state\": { \"filter\": \"nearest\", \"mip_filter\": \"none\""
				+ ", \"address_mode\": { \"u\": \"clamp_to_edge\", \"v\": \"clamp_to_edge\""
				+ ", \"w\": \"clamp_to_edge\" } } },\n";
		}
	}

	// A Metal entry point returns what it produces; a Vulkan one has no return
	// value and writes to an output variable instead, which the pipeline reads.
	// A returned struct becomes one output variable per member, each placed by
	// the attribute on that member, since that is the interface the struct
	// describes. Anything else becomes a single output.
	std::vector<Emitter::Output> Emitter::declareOutputs(const FunctionDecl& entryPoint) {
		std::vector<Output> outputs;
		const Type& returnType = entryPoint.returnType;

		if (returnType.isVoid()) {
			return outputs;
		}

		if (returnType.isSampler || returnType.textureDim != TextureDim::None
			|| returnType.isPointer) {
			throw CompileError("\"" + entryPoint.name + "\" returns a resource, which is not "
				"an output");
		}

		// The location an output without an attribute of its own takes. Counted
		// rather than fixed at zero so a source with more than one gets distinct
		// locations.
		uint32_t nextLocation = 0;

		if (returnType.namedType.empty()) {
			const Id type = declaredTypeOf(returnType);

			_builder.setSection(spirv::Section::TypesGlobals);
			const Id pointerType = _types.pointer(spirv::StorageClass::Output, type);
			const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
				{ static_cast<uint32_t>(spirv::StorageClass::Output) });

			_builder.setSection(spirv::Section::Annotations);
			_builder.emit(spirv::OpDecorate, { id,
				static_cast<uint32_t>(spirv::Decoration::Location), nextLocation++ });

			_interface.push_back(id);

			Output output;
			output.variable = id;
			output.type = type;
			outputs.push_back(output);
			return outputs;
		}

		const StructDecl* decl = _unit.findStruct(returnType.namedType);
		if (!decl) {
			throw CompileError("\"" + entryPoint.name + "\" returns \""
				+ returnType.namedType + "\", which is not a struct this source declares");
		}

		for (const StructField& field: decl->fields) {
			const Id type = declaredTypeOf(field.type);

			_builder.setSection(spirv::Section::TypesGlobals);
			const Id pointerType = _types.pointer(spirv::StorageClass::Output, type);
			const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
				{ static_cast<uint32_t>(spirv::StorageClass::Output) });

			_builder.setSection(spirv::Section::Annotations);
			if (field.attributes.position) {
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(spirv::BuiltIn::Position) });
			} else if (field.attributes.attributeIndex) {
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::Location), *field.attributes.attributeIndex });
			} else {
				// Metal gives a member with no attribute the next free location
				// rather than making it an error, so a struct's members are laid
				// out in declaration order across the locations a [[position]] and
				// the explicit attributes did not take.
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::Location), nextLocation++ });
			}

			_interface.push_back(id);

			Output output;
			output.variable = id;
			output.type = type;
			output.member = static_cast<int>(&field - decl->fields.data());
			outputs.push_back(output);
		}

		return outputs;
	}

	void Emitter::declareParameters(const FunctionDecl& entryPoint) {
		const std::vector<const Parameter*> implicitlyBound = assignImplicitBindings(entryPoint);

		for (size_t index = 0; index < entryPoint.parameters.size(); ++index) {
			const Parameter& parameter = entryPoint.parameters[index];

			if (parameter.attributes.builtin) {
				const BuiltinInput input = builtinInputFor(*parameter.attributes.builtin);
				const Id typeId = input.width > 1
					? _types.vector(input.scalar, input.width)
					: _types.scalar(input.scalar);

				// A builtin that is one scalar has no component to extract, so a
				// vector declared for one would be a type the source never asked
				// for and the body could not use.
				if (input.width == 1 && !parameter.type.isScalar()) {
					throw CompileError("parameter \"" + parameter.name + "\" is declared as a "
						"vector, but the builtin it takes is a single value");
				}

				_builder.setSection(spirv::Section::TypesGlobals);
				const Id pointerType = _types.pointer(spirv::StorageClass::Input, typeId);
				const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
					{ static_cast<uint32_t>(spirv::StorageClass::Input) });

				_builder.setSection(spirv::Section::Annotations);
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(input.builtin) });

				Binding binding;
				binding.id = id;
				binding.isPointer = true;
				binding.pointeeType = typeId;
				binding.storageClass = spirv::StorageClass::Input;

				// A three-component builtin declared as a scalar in MSL is that
				// builtin's first component: "uint index [[thread_position_in_grid]]"
				// is GlobalInvocationId.x, not the whole vector.
				binding.scalarComponentOfVector = input.width > 1 && parameter.type.isScalar();

				_bindings[parameter.name] = binding;
				_interface.push_back(id);
				continue;
			}

			// A [[stage_in]] parameter is the interface a stage receives, and
			// each of its members is a variable of its own placed by the
			// attribute on that member. The struct itself is not a value, since
			// SPIR-V has no interface struct to hold them.
			if (parameter.attributes.stageIn) {
				if (parameter.type.namedType.empty()) {
					throw CompileError("parameter \"" + parameter.name + "\" is [[stage_in]] but "
						"is not a struct type");
				}

				const StructDecl* decl = _unit.findStruct(parameter.type.namedType);
				if (!decl) {
					throw CompileError("parameter \"" + parameter.name + "\" is [[stage_in]] on \""
						+ parameter.type.namedType + "\", which is not a struct this source declares");
				}

				if (entryPoint.stage != Stage::Fragment) {
					throw CompileError("a [[stage_in]] parameter is what a fragment entry point "
						"receives, and \"" + entryPoint.name + "\" is not one");
				}

				Binding binding;
				binding.storageClass = spirv::StorageClass::Input;
				binding.structType = decl;

				// The same counter the output side uses, so a member with no
				// attribute of its own lands where the producing stage put it.
				uint32_t nextLocation = 0;

				for (const StructField& field: decl->fields) {
					const Id type = declaredTypeOf(field.type);

					_builder.setSection(spirv::Section::TypesGlobals);
					const Id pointerType = _types.pointer(spirv::StorageClass::Input, type);
					const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
						{ static_cast<uint32_t>(spirv::StorageClass::Input) });

					_builder.setSection(spirv::Section::Annotations);
					if (field.attributes.position) {
						// The position a fragment receives is its own coordinate,
						// not the position builtin, which the specification
						// reserves for the stages that produce one.
						_builder.emit(spirv::OpDecorate, { id,
							static_cast<uint32_t>(spirv::Decoration::BuiltIn),
							static_cast<uint32_t>(spirv::BuiltIn::FragCoord) });
					} else if (field.attributes.attributeIndex) {
						_builder.emit(spirv::OpDecorate, { id,
							static_cast<uint32_t>(spirv::Decoration::Location), *field.attributes.attributeIndex });
					} else {
						// As with an output, Metal gives a member with no attribute the
						// next free location rather than making it an error, and the
						// two sides of an interface have to agree on which member is
						// which. Both count up in declaration order, so the same
						// struct received and returned lands the same way round.
						_builder.emit(spirv::OpDecorate, { id,
							static_cast<uint32_t>(spirv::Decoration::Location),
							nextLocation++ });
					}

					_interface.push_back(id);
					binding.stageInMembers.push_back(id);
				}

				binding.isStageIn = true;
				_bindings[parameter.name] = binding;
				continue;
			}

			// A texture parameter is a descriptor of its own rather than a
			// pointer, and its Metal index is its binding as it is for a buffer.
			if (parameter.attributes.textureIndex) {
				if (parameter.type.textureDim == TextureDim::None) {
					throw CompileError("parameter \"" + parameter.name + "\" has [[texture("
						+ std::to_string(*parameter.attributes.textureIndex)
						+ ")]] but is not a texture type");
				}

				const Id imageType = _types.sampledImage(parameter.type.scalar, parameter.type.textureDim);

				_builder.setSection(spirv::Section::TypesGlobals);
				const Id pointerType = _types.pointer(spirv::StorageClass::UniformConstant, imageType);
				const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
					{ static_cast<uint32_t>(spirv::StorageClass::UniformConstant) });

				_builder.setSection(spirv::Section::Annotations);
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::DescriptorSet), 0u });
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::Binding), *parameter.attributes.textureIndex });

				Binding binding;
				binding.id = id;
				binding.isPointer = true;
				binding.pointeeType = imageType;
				binding.storageClass = spirv::StorageClass::UniformConstant;
				_bindings[parameter.name] = binding;

				_entryBindings += "\t\t{ \"kind\": \"Texture\", \"metal_index\": "
					+ std::to_string(*parameter.attributes.textureIndex)
					+ ", \"descriptor\": { \"set\": 0, \"binding\": "
					+ std::to_string(*parameter.attributes.textureIndex) + " }"
					+ ", \"param_index\": " + std::to_string(index)
					+ ", \"name\": \"" + parameter.name + "\" },\n";
				continue;
			}

			// A sampler parameter is a descriptor of its own, exactly as a texture
			// parameter is, in a set of its own because it shares a Metal index with
			// the texture it goes with.
			if (parameter.attributes.samplerIndex) {
				_builder.setSection(spirv::Section::TypesGlobals);
				const Id samplerType = _types.sampler();
				const Id pointerType = _types.pointer(spirv::StorageClass::UniformConstant, samplerType);
				const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
					{ static_cast<uint32_t>(spirv::StorageClass::UniformConstant) });

				_builder.setSection(spirv::Section::Annotations);
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::DescriptorSet), kSamplerParameterSet });
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::Binding),
					*parameter.attributes.samplerIndex });

				Binding binding;
				binding.id = id;
				binding.isPointer = true;
				binding.pointeeType = samplerType;
				binding.storageClass = spirv::StorageClass::UniformConstant;
				_bindings[parameter.name] = binding;

				_entryBindings += "\t\t{ \"kind\": \"Sampler\", \"metal_index\": "
					+ std::to_string(*parameter.attributes.samplerIndex)
					+ ", \"descriptor\": { \"set\": " + std::to_string(kSamplerParameterSet)
					+ ", \"binding\": " + std::to_string(*parameter.attributes.samplerIndex) + " }"
					+ ", \"param_index\": " + std::to_string(index)
					+ ", \"name\": \"" + parameter.name + "\" },\n";
				continue;
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

			const auto storageClass = storageClassForAddressSpace(parameter.type.addressSpace);
			if (!storageClass) {
				throw CompileError("parameter \"" + parameter.name + "\" needs a device, constant "
					"or threadgroup address space to be a buffer binding");
			}

		Id pointeeType = InvalidId;
		const StructDecl* structType = nullptr;

		if (!parameter.type.namedType.empty()) {
			structType = _unit.findStruct(parameter.type.namedType);
			if (!structType) {
				throw CompileError("parameter \"" + parameter.name + "\" refers to undeclared "
					"type \"" + parameter.type.namedType + "\"");
			}

			// A pointer to a struct is a buffer of that struct, so its element is
			// the struct laid out for an array and the wrapper below is what
			// carries the buffer. A struct reached directly is the buffer itself,
			// so it is the Block-decorated form.
			pointeeType = parameter.type.isPointer
				? _types.arrayElementStruct(parameter.type.namedType)
				: _types.blockStruct(parameter.type.namedType);
		} else {
			pointeeType = declaredTypeOf(parameter.type);
		}

		// A pointer parameter is a whole buffer, and Vulkan only accepts a
		// Block-decorated struct for a descriptor, so the element type is
		// wrapped in { T runtime_array[] } and the parameter indexes through
		// that.
		Id descriptorType = pointeeType;
		bool isBuffer = false;
		if (parameter.type.isPointer) {
			descriptorType = _types.blockStructFor(pointeeType);
			isBuffer = true;
		}

			_builder.setSection(spirv::Section::TypesGlobals);
			const Id pointerType = _types.pointer(*storageClass, descriptorType);
			const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
				{ static_cast<uint32_t>(*storageClass) });

			_builder.setSection(spirv::Section::Annotations);
			_builder.emit(spirv::OpDecorate, { id,
				static_cast<uint32_t>(spirv::Decoration::DescriptorSet), 0u });
			_builder.emit(spirv::OpDecorate, { id,
				static_cast<uint32_t>(spirv::Decoration::Binding), bindingIndex });

			Binding binding;
			binding.id = id;
			binding.isPointer = true;
			binding.pointeeType = pointeeType;
			binding.storageClass = *storageClass;
			binding.structType = structType;
			binding.isBuffer = isBuffer;
			_bindings[parameter.name] = binding;
			// SPIR-V 1.3 restricts the entry point's interface list to Input and
			// Output variables; 1.4 widened it to every global the entry point
			// statically uses. kSpirvVersion is 1.3, so a descriptor stays out of
			// the list and is reached through its DescriptorSet and Binding
			// decorations instead. Listing one is what spirv-val rejects.
			if (*storageClass == spirv::StorageClass::Input
				|| *storageClass == spirv::StorageClass::Output) {
				_interface.push_back(id);
			}

			_entryBindings += "\t\t{ \"kind\": \"Buffer\", \"metal_index\": "
				+ std::to_string(bindingIndex)
				+ ", \"descriptor\": { \"set\": 0, \"binding\": "
				+ std::to_string(bindingIndex) + " }"
				+ ", \"param_index\": " + std::to_string(index)
				+ ", \"name\": \"" + parameter.name + "\" },\n";
		}
	}

	// A local variable becomes a Function-storage pointer, which the
	// expression path then loads from, so a local and a parameter behave the
	// same way when a name is used.
	void Emitter::emitVariableDeclaration(const VariableDeclaration& declaration) {
		const Id typeId = declaredTypeOf(declaration.type);

		// A function's variables have to be the first instructions of its first
		// block, so the variable is declared before its initialiser is computed.
		const Id pointerType = _types.pointer(spirv::StorageClass::Function, typeId);
		const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });

		Id initial = InvalidId;
		if (declaration.initializer) {
			const Id initializer = emitExpression(*declaration.initializer);
			initial = convert(initializer, _builder.typeOf(initializer), typeId);
		} else {
			// OpConstantNull rather than an OpConstant of zero, which takes a
			// scalar only and so cannot spell the zero of a vector or a struct.
			initial = _builder.emitDeclTyped(spirv::OpConstantNull, typeId, { });
		}

		_builder.emit(spirv::OpStore, { id, initial });

		Binding binding;
		binding.id = id;
		binding.isPointer = true;
		binding.pointeeType = typeId;
		binding.storageClass = spirv::StorageClass::Function;

		// A local of a struct type, so a member of it can be resolved the way
		// one of a parameter's is.
		if (!declaration.type.namedType.empty()) {
			binding.structType = _unit.findStruct(declaration.type.namedType);
		}

		_bindings[declaration.name] = binding;
	}

	void Emitter::emitStatement(const Statement& statement) {
		switch (statement.kind) {
			case StatementKind::Compound:
				for (const StatementPtr& child: statement.children) {
					emitStatement(*child);
				}
				return;

			case StatementKind::ExpressionStatement:
				if (statement.expression && !emitStore(*statement.expression)) {
					emitExpression(*statement.expression);
				}
				return;

			case StatementKind::DeclarationStatement:
				emitVariableDeclaration(*statement.declaration);
				return;

			case StatementKind::Return:
				if (statement.expression) {
					// The value an entry point returns is written to its outputs,
					// since it has no return value of its own. A returned struct
					// supplies one member per output.
					const Id value = emitExpression(*statement.expression);
					for (const Output& output: _outputs) {
						const Id member = output.member < 0
							? value
							: _builder.emitTyped(spirv::OpCompositeExtract, output.type,
								{ value, static_cast<uint32_t>(output.member) });

						_builder.emit(spirv::OpStore, { output.variable,
							convert(member, _builder.typeOf(member), output.type) });
					}

					if (_outputs.empty()) {
						throw CompileError("a return with a value needs an entry point that "
							"returns one");
					}
				}

				emitTerminator(spirv::OpReturn, { });
				return;

			case StatementKind::If: {
				const Id condition = emitExpression(*statement.expression);
				const Id thenLabel = _builder.nextId();
				const Id elseLabel = _builder.nextId();
				const Id mergeLabel = _builder.nextId();

				// A selection is only valid when the header declares the block it
				// joins, which is what makes the control flow structured rather than
				// a graph the validator cannot check. The declaration is the second
				// to last instruction of the header, so it goes before the branch
				// rather than after it. Zero is the selection control, the
				// enumerant for "none of it": a shader has no flat shading to
				// select.
				_builder.emit(spirv::OpSelectionMerge, { mergeLabel, 0u });
				emitTerminator(spirv::OpBranchConditional, { condition, thenLabel, elseLabel });

				emitLabel(thenLabel);
				emitStatement(*statement.thenBranch);
				emitTerminator(spirv::OpBranch, { mergeLabel });
				emitLabel(elseLabel);
				if (statement.elseBranch) {
					emitStatement(*statement.elseBranch);
				}
				emitTerminator(spirv::OpBranch, { mergeLabel });
				emitLabel(mergeLabel);
				return;
			}

			case StatementKind::For: {
				if (statement.forInitializer) {
					emitVariableDeclaration(*statement.forInitializer);
				} else if (statement.expression) {
					emitExpression(*statement.expression);
				}

				const Id condLabel = _builder.nextId();
				const Id bodyLabel = _builder.nextId();
				const Id continueLabel = _builder.nextId();
				const Id mergeLabel = _builder.nextId();

				emitTerminator(spirv::OpBranch, { condLabel });
				emitLabel(condLabel);

				// The loop header declares the block it leaves to and the block the
				// back edge runs through, again as the second to last instruction.
				// The continue block is the label the increment runs in, which is
				// what makes the back edge a continue rather than a branch that
				// skips it. Zero is the loop control, the enumerant for "none of
				// it": mslc does not unroll.
				// The condition is computed before the declaration rather than
				// after it, since the declaration has to be the second to last
				// instruction of the header and therefore immediately before the
				// branch.
				Id condition = InvalidId;
				if (statement.forCondition) {
					condition = emitExpression(*statement.forCondition);
				}

				_builder.emit(spirv::OpLoopMerge, { mergeLabel, continueLabel, 0u });
				if (statement.forCondition) {
					emitTerminator(spirv::OpBranchConditional, { condition, bodyLabel, mergeLabel });
				} else {
					emitTerminator(spirv::OpBranch, { bodyLabel });
				}

				emitLabel(bodyLabel);
				emitStatement(*statement.forBody);
				emitTerminator(spirv::OpBranch, { continueLabel });

				emitLabel(continueLabel);
				if (statement.forIncrement && !emitStore(*statement.forIncrement)) {
					emitExpression(*statement.forIncrement);
				}
				emitTerminator(spirv::OpBranch, { condLabel });

				emitLabel(mergeLabel);
				return;
			}

			case StatementKind::While: {
				const Id condLabel = _builder.nextId();
				const Id bodyLabel = _builder.nextId();
				// A while loop needs a continue block of its own even though the
				// source has nothing to run at the end of an iteration: a loop's
				// back edge has to run through the block the header declared as
				// the continue, or the body is not structured.
				const Id continueLabel = _builder.nextId();
				const Id mergeLabel = _builder.nextId();

				emitTerminator(spirv::OpBranch, { condLabel });
				emitLabel(condLabel);

				const Id condition = emitExpression(*statement.whileCondition);
				_builder.emit(spirv::OpLoopMerge, { mergeLabel, continueLabel, 0u });
				emitTerminator(spirv::OpBranchConditional, { condition, bodyLabel, mergeLabel });

				emitLabel(bodyLabel);
				emitStatement(*statement.whileBody);
				emitTerminator(spirv::OpBranch, { continueLabel });

				emitLabel(continueLabel);
				emitTerminator(spirv::OpBranch, { condLabel });

				emitLabel(mergeLabel);
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

	EmittedModule Emitter::run() {
		_builder.setSection(spirv::Section::Capabilities);
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Shader) });

		_builder.setSection(spirv::Section::MemoryModel);
		_builder.emit(spirv::OpMemoryModel,
			{ static_cast<uint32_t>(spirv::AddressingModel::Logical),
			  static_cast<uint32_t>(spirv::MemoryModel::GLSL450) });

		_uintType = _types.scalar(ScalarKind::UInt);
		_intType = _types.scalar(ScalarKind::Int);
		_boolType = _types.scalar(ScalarKind::Bool);
		_voidType = _types.voidType();

		declareGlobals();

		EmittedModule module;
		for (const FunctionDecl* entryPoint: _entryPoints) {
			module.entries.push_back(emitEntryPoint(*entryPoint));
		}

		module.moduleBindings = _moduleBindings;
		return module;
	}

	// Vulkan's entry points all return void: what a Metal entry point returns
	// becomes an output variable, which is the next step. Rejecting it here
	// keeps the module honest rather than emitting a return value no Vulkan
	// pipeline would accept.
	EmittedEntryPoint Emitter::emitEntryPoint(const FunctionDecl& entryPoint) {
		_bindings.clear();
		_interface.clear();
		_entryBindings.clear();
		_outputs.clear();
		_blockTerminated = false;

		spirv::ExecutionModelValue model = spirv::ExecutionModel::GLCompute;
		if (entryPoint.stage == Stage::Vertex) {
			model = spirv::ExecutionModel::Vertex;
		} else if (entryPoint.stage == Stage::Fragment) {
			model = spirv::ExecutionModel::Fragment;
		}

		declareParameters(entryPoint);
		_outputs = declareOutputs(entryPoint);

		// The entry point's own id has to exist before it is named in
		// OpEntryPoint, so it is allocated here rather than after.
		const Id entryPointId = _builder.nextId();

		// An entry point's parameters are globals rather than function
		// parameters, so the signature is the return type alone.
		_builder.setSection(spirv::Section::TypesGlobals);
		const Id functionType = _types.functionType(_voidType, { });

		_builder.setSection(spirv::Section::EntryPoints);
		{
			std::vector<uint32_t> operands = {
				static_cast<uint32_t>(model), entryPointId
			};
			spirv::Builder::appendString(operands, entryPoint.name);
			for (Id id: _interface) {
				operands.push_back(id);
			}
			_builder.emit(spirv::OpEntryPoint, operands);
		}

		if (entryPoint.stage == Stage::Kernel) {
			_builder.setSection(spirv::Section::ExecutionModes);
			_builder.emit(spirv::OpExecutionMode, { entryPointId,
				static_cast<uint32_t>(spirv::ExecutionMode::LocalSize),
				_options.localSizeX, _options.localSizeY, _options.localSizeZ });
		} else if (entryPoint.stage == Stage::Fragment) {
			// A fragment entry point with no origin is not loadable. Metal's
			// fragment stage has a fixed origin and no counterpart to the MSL
			// argument that would move it, so this is the only origin mslc emits.
			_builder.setSection(spirv::Section::ExecutionModes);
			_builder.emit(spirv::OpExecutionMode, { entryPointId,
				static_cast<uint32_t>(spirv::ExecutionMode::OriginUpperLeft) });
		}

		_builder.setSection(spirv::Section::Functions);
		// The function's own id has to be the one the entry point names, so it
		// is written explicitly rather than taken from the emit's return.
		_builder.emitDeclTypedAt(spirv::OpFunction, _voidType, entryPointId,
			{ _intType, functionType });
		emitLabel(_builder.nextId());

		// A function's variables are only valid among the first instructions of
		// its first block, and a body discovers one at its declaration, so the
		// variables are buffered while the body is emitted and spliced in behind
		// the opening label. Only an entry point's own body is lowered, so the
		// prologue is opened and closed around it rather than per function.
		_builder.openPrologue();
		emitFunctionBody(*entryPoint.body);
		_builder.closePrologue();

		// A body that returns on every path has already ended the block, and a
		// terminator after one belongs to no block at all.
		if (!_blockTerminated) {
			emitTerminator(spirv::OpReturn, { });
		}

		_builder.emit(spirv::OpFunctionEnd, { });

		EmittedEntryPoint result;
		result.stage = entryPoint.stage;
		result.name = entryPoint.name;
		result.bindings = _entryBindings;
		return result;
	}

}

EmittedModule emitModule(spirv::Builder& builder, const TranslationUnit& unit,
	const std::vector<const FunctionDecl*>& entryPoints, const ModuleOptions& options) {
	TypeTable types(builder, unit);
	Emitter emitter(builder, unit, entryPoints, options, types);
	return emitter.run();
}

}
