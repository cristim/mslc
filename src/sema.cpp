#include "sema.h"

#include "lexer.h"
#include "parser.h"

#include <algorithm>
#include <cstring>
#include <set>

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

	// A packed vector is its components back to back, aligned as a component is: a
	// packed_float3 is 12 bytes at any multiple of 4, where a float3 is 16 at a
	// multiple of 16.
	VectorLayout packedVectorLayoutFor(uint32_t scalarBytes, uint32_t components) {
		return { components * scalarBytes, scalarBytes };
	}

	// A matrix is its columns laid out as an array of column vectors, so a
	// float3x3 is three 16-byte columns, 48 bytes, and not nine packed floats.
	// The column's own size is the MatrixStride.
	VectorLayout matrixLayoutFor(uint32_t scalarBytes, uint32_t columns, uint32_t rows) {
		const VectorLayout column = vectorLayoutFor(scalarBytes, rows);
		return { columns * column.size, column.alignment };
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

	// How a math builtin's arguments relate to each other and to its result.
	enum class MathShape {
		// Every argument is the result type, or a scalar broadcast to it.
		Componentwise,
		// FClamp(x, 0, 1).
		Saturate,
		// Vectors of one type in, that type out: normalize, reflect.
		Vector,
		// Vectors of one type in, their component out: length, distance.
		VectorToScalar,
		// As VectorToScalar, but a core opcode rather than GLSL.std.450.
		Dot,
		// Two vectors of one type and a scalar eta.
		Refract,
		// Two three-component vectors.
		Cross,
	};

	// GLSL.std.450 numbers its instructions from 1, so 0 marks a builtin with no
	// integer form.
	constexpr uint32_t kNoInstruction = 0;

	struct MathBuiltin {
		const char* name;
		size_t arity;
		MathShape shape;
		// GLSL.std.450 instruction numbers, from the extended instruction set's
		// grammar. A float-only builtin has no signed or unsigned instruction.
		uint32_t floatInstruction;
		uint32_t signedInstruction;
		uint32_t unsignedInstruction;
	};

	constexpr MathBuiltin kMathBuiltins[] = {
		{ "trunc", 1, MathShape::Componentwise, 3, kNoInstruction, kNoInstruction },
		{ "abs", 1, MathShape::Componentwise, 4, 5, kNoInstruction },
		{ "floor", 1, MathShape::Componentwise, 8, kNoInstruction, kNoInstruction },
		{ "ceil", 1, MathShape::Componentwise, 9, kNoInstruction, kNoInstruction },
		{ "fract", 1, MathShape::Componentwise, 10, kNoInstruction, kNoInstruction },
		{ "sin", 1, MathShape::Componentwise, 13, kNoInstruction, kNoInstruction },
		{ "cos", 1, MathShape::Componentwise, 14, kNoInstruction, kNoInstruction },
		{ "tan", 1, MathShape::Componentwise, 15, kNoInstruction, kNoInstruction },
		{ "asin", 1, MathShape::Componentwise, 16, kNoInstruction, kNoInstruction },
		{ "acos", 1, MathShape::Componentwise, 17, kNoInstruction, kNoInstruction },
		{ "atan", 1, MathShape::Componentwise, 18, kNoInstruction, kNoInstruction },
		{ "atan2", 2, MathShape::Componentwise, 25, kNoInstruction, kNoInstruction },
		{ "pow", 2, MathShape::Componentwise, 26, kNoInstruction, kNoInstruction },
		{ "exp", 1, MathShape::Componentwise, 27, kNoInstruction, kNoInstruction },
		{ "log", 1, MathShape::Componentwise, 28, kNoInstruction, kNoInstruction },
		{ "exp2", 1, MathShape::Componentwise, 29, kNoInstruction, kNoInstruction },
		{ "log2", 1, MathShape::Componentwise, 30, kNoInstruction, kNoInstruction },
		{ "sqrt", 1, MathShape::Componentwise, 31, kNoInstruction, kNoInstruction },
		{ "rsqrt", 1, MathShape::Componentwise, 32, kNoInstruction, kNoInstruction },
		{ "min", 2, MathShape::Componentwise, 37, 39, 38 },
		{ "max", 2, MathShape::Componentwise, 40, 42, 41 },
		{ "clamp", 3, MathShape::Componentwise, 43, 45, 44 },
		{ "saturate", 1, MathShape::Saturate, 43, kNoInstruction, kNoInstruction },
		{ "mix", 3, MathShape::Componentwise, 46, kNoInstruction, kNoInstruction },
		{ "step", 2, MathShape::Componentwise, 48, kNoInstruction, kNoInstruction },
		{ "smoothstep", 3, MathShape::Componentwise, 49, kNoInstruction, kNoInstruction },
		{ "fma", 3, MathShape::Componentwise, 50, kNoInstruction, kNoInstruction },
		{ "length", 1, MathShape::VectorToScalar, 66, kNoInstruction, kNoInstruction },
		{ "distance", 2, MathShape::VectorToScalar, 67, kNoInstruction, kNoInstruction },
		{ "cross", 2, MathShape::Cross, 68, kNoInstruction, kNoInstruction },
		{ "normalize", 1, MathShape::Vector, 69, kNoInstruction, kNoInstruction },
		{ "reflect", 2, MathShape::Vector, 71, kNoInstruction, kNoInstruction },
		{ "refract", 3, MathShape::Refract, 72, kNoInstruction, kNoInstruction },
		{ "dot", 2, MathShape::Dot, kNoInstruction, kNoInstruction, kNoInstruction },
	};

	const MathBuiltin* findMathBuiltin(const std::string& name) {
		for (const MathBuiltin& builtin: kMathBuiltins) {
			if (name == builtin.name) {
				return &builtin;
			}
		}

		return nullptr;
	}

	// The merge and function control masks. Every one of them is None: Pure
	// would promise no memory writes, which an entry point that stores to a
	// buffer breaks, and neither flattening nor unrolling changes what the
	// shader computes. Named from the generated table rather than written as 0.
	constexpr uint32_t kFunctionControlNone = spirv::FunctionControl::None;
	constexpr uint32_t kSelectionControlNone = spirv::SelectionControl::None;
	constexpr uint32_t kLoopControlNone = spirv::LoopControl::None;
	// The Lod bit of the image operands mask, which an explicit-lod sample and
	// an image fetch both carry.
	constexpr uint32_t kImageOperandsLod = 0x2;

	// Every name an expression or statement refers to. A constexpr sampler nothing
	// refers to has no state in Apple's AIR, so it gets no binding either.
	void collectIdentifiers(const Expression& expression, std::set<std::string>& names) {
		if (expression.kind == ExpressionKind::Identifier) {
			names.insert(expression.name);
		}
		for (const Expression* child: { expression.left.get(), expression.right.get() }) {
			if (child) collectIdentifiers(*child, names);
		}
		for (const ExpressionPtr& argument: expression.arguments) {
			collectIdentifiers(*argument, names);
		}
		for (const InitializerElement& element: expression.elements) {
			collectIdentifiers(*element.value, names);
		}
	}

	void collectIdentifiers(const Statement& statement, std::set<std::string>& names) {
		for (const Expression* expression: { statement.expression.get(), statement.forCondition.get(),
			statement.forIncrement.get(), statement.whileCondition.get() }) {
			if (expression) collectIdentifiers(*expression, names);
		}
		for (const std::optional<VariableDeclaration>* declaration: { &statement.declaration, &statement.forInitializer }) {
			if (*declaration && (*declaration)->initializer) {
				collectIdentifiers(*(*declaration)->initializer, names);
			}
		}
		for (const StatementPtr& child: statement.children) {
			collectIdentifiers(*child, names);
		}
		for (const Statement* nested: { statement.thenBranch.get(), statement.elseBranch.get(),
			statement.forBody.get(), statement.whileBody.get() }) {
			if (nested) collectIdentifiers(*nested, names);
		}
	}

	uint16_t comparisonOpcode(BinaryOperator op, bool isFloat, bool isSigned) {
		using Op = uint16_t;

		if (isFloat) {
			switch (op) {
				case BinaryOperator::Equal: return spirv::OpFOrdEqual;
				// Unordered, so NaN != x is true; the other five are false for a NaN.
				case BinaryOperator::NotEqual: return spirv::OpFUnordNotEqual;
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

// The types a struct's members have where the struct is laid out, which are the
// value types except for the packed ones.
std::vector<Id> TypeTable::storageTypesFor(const std::string& name, const std::vector<Id>& valueTypes) {
	std::vector<Id> types = valueTypes;
	const StructDecl* decl = _unit.findStruct(name);
	for (size_t i = 0; i < types.size(); ++i) {
		const Type& field = decl->fields[i].type;
		if (field.isPacked) {
			types[i] = packedStorage(field.scalar, field.vectorWidth);
		}
	}

	return types;
}

const TypeTable::MatrixInfo* TypeTable::matrixInfo(Id type) const {
	const auto it = _matrixInfo.find(type);
	return it == _matrixInfo.end() ? nullptr : &it->second;
}

void TypeTable::decorateMatrixMember(Id structure, uint32_t member, Id type) {
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
		stride = it->second;
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
	if (_widthOfVector.count(type) > 0 || _matrixInfo.count(type) > 0) {
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

uint32_t TypeTable::alignmentOf(Id type, bool packed) const {
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
	if (packed) {
		return std::max<uint32_t>(1, packedVectorLayoutFor(scalarBytes, count).alignment);
	}

	return std::max<uint32_t>(1, vectorLayoutFor(scalarBytes, count).alignment);
}

Id TypeTable::zero(Id type) {
	// OpConstant is a scalar form; OpConstantNull zeroes every component and
	// member of a vector, matrix or struct.
	if (isAggregate(type)) {
		return _builder.emitDeclTyped(spirv::OpConstantNull, type, { });
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

Id TypeTable::one(Id type) {
	const Id vectorComponent = componentOf(type);
	const Id component = vectorComponent == InvalidId ? type : vectorComponent;
	const uint32_t bits = bitWidth(component);

	std::vector<uint32_t> words;
	if (isFloat(component)) {
		// 1.0 as a half is 0x3C00 and as a double 0x3FF0 in the high word.
		words = bits == 16 ? std::vector<uint32_t>{ 0x3C00u }
			: bits == 32 ? std::vector<uint32_t>{ 0x3F800000u }
			: std::vector<uint32_t>{ 0u, 0x3FF00000u };
	} else {
		words = std::vector<uint32_t>((bits + 31) / 32, 0u);
		words.front() = 1u;
	}

	const Id scalarOne = _builder.emitDeclTyped(spirv::OpConstant, component, words);
	if (component == type) {
		return scalarOne;
	}

	return _builder.emitDeclTyped(spirv::OpConstantComposite, type,
		std::vector<uint32_t>(vectorWidth(type), scalarOne));
}

Id TypeTable::image2D() {
	if (_image2D == InvalidId) {
		_image2D = _builder.emitDecl(spirv::OpTypeImage, { scalar(ScalarKind::Float),
			static_cast<uint32_t>(spirv::Dim::Dim2D), 2u, 0u, 0u, 1u,
			static_cast<uint32_t>(spirv::ImageFormat::Unknown) });
	}

	return _image2D;
}

Id TypeTable::samplerType() {
	if (_samplerType == InvalidId) {
		_samplerType = _builder.emitDecl(spirv::OpTypeSampler);
	}

	return _samplerType;
}

Id TypeTable::sampledImage2D() {
	if (_sampledImage2D == InvalidId) {
		_sampledImage2D = _builder.emitDecl(spirv::OpTypeSampledImage, { image2D() });
	}

	return _sampledImage2D;
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
		const uint32_t scalarBytes = mappingFor(field.type.scalar).width / 8;
		const VectorLayout layout = field.type.isMatrix()
			? matrixLayoutFor(scalarBytes, field.type.matrixColumns, components)
			: field.type.isPacked ? packedVectorLayoutFor(scalarBytes, components)
			: vectorLayoutFor(scalarBytes, components);

		outTypes.push_back(field.type.isMatrix()
			? matrix(field.type.scalar, field.type.matrixColumns, field.type.vectorWidth)
			: components > 1
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

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, storageTypesFor(name, fieldTypes));
	for (size_t i = 0; i < fieldTypes.size(); ++i) {
		_builder.emit(spirv::OpMemberDecorate, { id, static_cast<uint32_t>(i),
			static_cast<uint32_t>(spirv::Decoration::Offset), offsets[i] });
		decorateMatrixMember(id, static_cast<uint32_t>(i), fieldTypes[i]);
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

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, storageTypesFor(name, fieldTypes));

	// A struct reached through a buffer binding is an interface block, so it
	// needs Block and per-member Offset. std140 rules apply, which for the
	// members the corpus has is Metal's own layout.
	_builder.emit(spirv::OpDecorate, { id, static_cast<uint32_t>(spirv::Decoration::Block) });
	for (size_t i = 0; i < fieldTypes.size(); ++i) {
		_builder.emit(spirv::OpMemberDecorate, { id, static_cast<uint32_t>(i),
			static_cast<uint32_t>(spirv::Decoration::Offset), offsets[i] });
		decorateMatrixMember(id, static_cast<uint32_t>(i), fieldTypes[i]);
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
		// True for a const local and for a buffer in constant memory or declared
		// const: Metal rejects a store through it.
		bool readOnly = false;
		// True for a sampler declared with coord::pixel. Vulkan forbids an
		// implicit-lod lookup through an unnormalized sampler.
		bool unnormalizedSampler = false;
	};

	// A constexpr sampler of the entry point being emitted. Samplers with equal
	// state share one variable, as Apple gives equal states one global.
	struct EmbeddedSampler {
		SamplerState state;
		Id variable = InvalidId;
	};

	// A name declared in a block or a for-loop header stops naming it when the
	// scope ends, and a name it hid names what it did before.
	class BindingScope {
	public:
		explicit BindingScope(std::map<std::string, Binding>& bindings)
			: _bindings(bindings), _saved(bindings) {}
		~BindingScope() { _bindings = std::move(_saved); }
		BindingScope(const BindingScope&) = delete;
		BindingScope& operator=(const BindingScope&) = delete;

	private:
		std::map<std::string, Binding>& _bindings;
		std::map<std::string, Binding> _saved;
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

	// An Input or Output variable carrying one value across a stage boundary: a
	// returned value, or one field of a returned or [[stage_in]] struct. The
	// variable's type can differ from the value's, because a half crosses as a
	// float.
	struct StageVariable {
		Id variable = InvalidId;
		Id interfaceType = InvalidId;
		Id valueType = InvalidId;
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
		// Addresses of a packed vector in a buffer. The pointee is the same SPIR-V
		// vector as a float3's, so the address is what remembers that it is only as
		// aligned as a component.
		std::set<Id> _packedAddresses;

		Id _uintType = InvalidId;
		Id _intType = InvalidId;
		Id _boolType = InvalidId;
		Id _voidType = InvalidId;
		Id _functionId = InvalidId;
		Id _entryPointId = InvalidId;
		spirv::Id _glslSet = InvalidId;

		// Set once the current block has its terminator, so nothing more may be
		// emitted into it until the next label.
		bool _terminated = false;

		std::string _reflection;
		// The next descriptor binding a texture or sampler takes, and the
		// constexpr samplers declared so far in the entry point.
		uint32_t _nextBinding = 0;
		std::vector<EmbeddedSampler> _embeddedSamplers;
		// Set once the module needs ImageQuery, which is declared once.
		bool _imageQueryDeclared = false;
		// Ids the entry point lists as its interface, in declaration order.
		std::vector<Id> _interface;

		// What a return statement writes: one variable for a returned scalar or
		// vector, or one per field of a returned struct. Empty for a void function.
		std::vector<StageVariable> _outputs;
		// The fragment function's [[stage_in]] parameter and one Input per field.
		const Parameter* _stageIn = nullptr;
		std::vector<StageVariable> _stageInputs;

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
		// The address of a packed vector's storage, viewed as a pointer to the
		// vector itself, which is what a load or store wants.
		Id packedVectorAddress(Id storageAddress, Id vectorType);
		Id loadFromBuffer(Id pointer, Id pointeeType);
		void storeIntoBuffer(Id pointer, Id value);
		// The Aligned memory operand a buffer access has to carry, at the
		// alignment the buffer's own layout guarantees.
		std::vector<uint32_t> alignedOperands(Id pointer, Id pointeeType) const;
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
		void declareResources(const std::vector<std::pair<size_t, const Parameter*>>& resources);
		Id declareDescriptorVariable(Id pointee, uint32_t binding);
		void reserveLocalSamplers(const Statement& statement, const std::set<std::string>& used);
		void declareLocalSampler(const VariableDeclaration& declaration);
		void addReflectionEntry(const std::string& entry);
		std::string descriptorJson(uint32_t binding) const;
		Id emitTextureCall(const Expression& call);
		Id emitTextureSample(const Expression& call, const Binding& texture);
		Id emitTextureRead(const Expression& call, const Binding& texture);
		Id emitTextureSize(const Expression& call, const Binding& texture, bool width);
		Id texturePixels(const Binding& texture, Id sampled);
		Id emitLod(const Expression& expression, const std::string& callName);
		const StructDecl* structValue(const Type& type) const;
		StageVariable declareStageVariable(const Type& type, spirv::StorageClassValue storageClass,
			const std::string& what);
		std::vector<StageVariable> declareStageStruct(const StructDecl& decl,
			spirv::StorageClassValue storageClass);
		void declareStageOutputs();
		void loadStageInputs();
		void emitReturn(const Statement& statement);
		void checkStageInterfacesAgree() const;
		void emitFunctionBody(const Statement& statement);
		void emitStatement(const Statement& statement);
		void emitExpressionStatement(const Expression& expression);
		Id emitPlaceAddress(const Expression& left);
		void emitSwizzleStore(const Expression& target, const Expression& valueExpression);
		void emitVariableDeclaration(const VariableDeclaration& declaration);
		void bindLocal(const std::string& name, Id variable, Id type, const Type& msl);
		void beginBlock(Id label);
		void terminate(uint16_t opcode, std::vector<uint32_t> operands);
		void branchUnlessTerminated(Id label);

		// Every expression returns a value id whose type the builder knows.
		// For an lvalue such as "buffer[index]" the result is a pointer, and
		// the caller loads from it.
		Id emitExpression(const Expression& expression);
		Id emitBinary(const Expression& expression);
		Id emitMatrixProduct(BinaryOperator op, Id left, Id right);
		Id emitUnary(const Expression& expression);
		Id emitIndex(const Expression& expression, bool asAddress);
		Id emitMember(const Expression& expression);
		Id emitMemberAddress(const Expression& expression, Id& outFieldType);

		// The file-scope constant an expression names, or null when it names
		// something else. A constant is a value and not a place, which is what
		// tells a member read on one apart from a member read on a local.
		const ConstantBinding* constantFor(const Expression* expression);
		Id emitCall(const Expression& expression);
		Id emitMathBuiltin(const MathBuiltin& builtin, const std::vector<ExpressionPtr>& arguments);
		Id emitConstruct(const Expression& expression);
		Id emitConstructList(const Expression& expression, Id toType);
		const StructDecl* structOf(const Expression& expression);
		Id emitSwizzle(const Expression& expression);
		Id emitIdentifier(const Expression& expression);
		Id loadFrom(Id pointer, Id pointeeType);
		Id promotedTo(uint32_t components) const;
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

	Id Emitter::loadFromBuffer(Id pointer, Id pointeeType) {
		std::vector<uint32_t> operands = alignedOperands(pointer, pointeeType);
		operands.insert(operands.begin(), pointer);
		return _builder.emitTyped(spirv::OpLoad, pointeeType, operands);

	}

	void Emitter::storeIntoBuffer(Id pointer, Id value) {
		const Id pointeeType = _types.pointeeOf(_builder.typeOf(pointer));

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
				? _builder.emitTyped(_types.isFloat(fromType)
						? spirv::OpFUnordNotEqual : spirv::OpINotEqual,
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

		if (binding.pointeeMsl.resource != ResourceKind::None) {
			throw CompileError("\"" + expression.name + "\" is a " + typeName(binding.pointeeMsl)
				+ ", which mslc uses as the receiver of a texture call or as sample's sampler "
					"argument only");
		}

		// A buffer is reached through the address block, so the binding has no id
		// of its own to load, and loading it wrote an OpLoad of id 0.
		if (binding.bufferPointeeType != InvalidId) {
			throw CompileError("the buffer \"" + expression.name + "\" is used as a value, "
				"which is not lowered yet");
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

	static const char* const kDereferenceOperand = "the operand of unary '*' has to be a pointer "
		"parameter; a dereference of an element, a call or other expression is not supported";

	static std::string notAPointer(const std::string& name) {
		return "\"" + name + "\" is not a pointer, so unary '*' cannot dereference it";
	}

	// Indexing is an lvalue: it produces the address of the element. Most uses
	// want the element, so the value is loaded by default and only an
	// assignment target asks for the address. Without this the operands of
	// "a[i] + b[i]" would be pointers, and the addition would be typed as
	// integer.
	Id Emitter::emitIndex(const Expression& expression, bool asAddress) {
		if (expression.left->kind != ExpressionKind::Identifier) {
			throw CompileError(expression.isDereference ? kDereferenceOperand
				: "indexing an expression is not supported yet; index a parameter or local directly");
		}

		const auto it = _bindings.find(expression.left->name);
		if (it == _bindings.end() || !it->second.isPointer) {
			throw CompileError("\"" + expression.left->name + "\" is not a pointer, so it cannot "
				"be indexed");
		}

		if (expression.isDereference && !it->second.isBuffer) {
			throw CompileError(notAPointer(expression.left->name));
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
			const bool packed = binding.pointeeMsl.isPacked;
			const Id resultType = _types.pointer(spirv::StorageClass::PhysicalStorageBuffer,
				packed ? _types.packedStorage(binding.pointeeMsl.scalar, binding.pointeeMsl.vectorWidth)
					: binding.pointeeType);

			// A buffer's pointee is { T runtime_array[] }, so an element is member
			// 0 and then the index. A struct pointee is indexed directly.
			Id address = binding.isBuffer
				? _builder.emitTyped(spirv::OpAccessChain, resultType,
					{ base, constantU32(0), index })
				: _builder.emitTyped(spirv::OpAccessChain, resultType, { base, index });

			if (packed) {
				address = packedVectorAddress(address, binding.pointeeType);
			}

			return asAddress ? address : loadFromBuffer(address, binding.pointeeType);
		}

		// Indexing a matrix names one of its columns, not another matrix.
		const auto* matrix = _types.matrixInfo(binding.pointeeType);
		const Id elementType = matrix ? matrix->column : binding.pointeeType;
		const Id address = _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(binding.storageClass, elementType),
			{ binding.id, index });

		return asAddress ? address : loadFrom(address, elementType);
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
		bool isDereference = false;
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

	// The struct an expression's value is, or null when it is not one. A '.' on
	// anything else is a swizzle, which is why this is asked before any code for
	// the left side is emitted. A member is never a struct: a struct field of
	// struct type is rejected where the struct is declared.
	const StructDecl* Emitter::structOf(const Expression& expression) {
		switch (expression.kind) {
			case ExpressionKind::Identifier: {
				const auto it = _bindings.find(expression.name);
				if (it != _bindings.end()) {
					const std::string& name = it->second.pointeeMsl.namedType;
					return name.empty() ? nullptr : _unit.findStruct(name);
				}
				const auto constant = _constants.find(expression.name);
				return constant == _constants.end() ? nullptr : constant->second.structType;
			}

			// An element of a buffer has the buffer's own type.
			case ExpressionKind::Index: return structOf(*expression.left);

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
		const std::vector<uint32_t> indices = swizzleLanes(name);

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

	Id Emitter::emitMember(const Expression& expression) {
		if (!structOf(*expression.left)) {
			return emitSwizzle(expression);
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
					if (step.isDereference) {
						throw CompileError(notAPointer(rootName));
					}
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

		// A packed member of a laid-out struct is stored as an array of its
		// components, and is viewed as the vector once it has an address.
		const bool packed = fromBuffer && current.isPacked;
		const Id address = _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(storageClass, packed
				? _types.packedStorage(current.scalar, current.vectorWidth) : outFieldType), operands);

		return packed ? packedVectorAddress(address, outFieldType) : address;
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

		// Only the column form, one vector per column. A single scalar is a
		// diagonal matrix and a list of scalars fills it element by element; both
		// are their own lowering and are reported rather than guessed at.
		if (target.isMatrix()) {
			if (expression.arguments.size() != target.matrixColumns) {
				throw CompileError(spelled + " built from " + std::to_string(expression.arguments.size())
					+ " values is not lowered yet; mslc builds a matrix from one "
					+ std::string(scalarKindName(target.scalar)) + std::to_string(target.vectorWidth)
					+ " per column");
			}

			std::vector<uint32_t> columns;
			for (const ExpressionPtr& argument: expression.arguments) {
				const Id column = emitExpression(*argument);
				if (_builder.typeOf(column) != _types.matrixInfo(toType)->column) {
					throw CompileError("a column of " + spelled + " has to be a "
						+ std::string(scalarKindName(target.scalar))
						+ std::to_string(target.vectorWidth) + " already; mslc converts no column");
				}
				columns.push_back(column);
			}

			return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
		}

		// float3() and float3(0) are different values and mslc has no value to put
		// in the components, so it says so rather than fabricating a zero. xcrun
		// metal accepts the empty form and zero-fills it; a caller cannot tell
		// that apart from float3(0) in the emitted module, which is the reason for
		// the diagnostic.
		if (expression.arguments.empty()) {
			throw CompileError(spelled + "() has no arguments and mslc has no default "
				"value to put in it; write the value you want, as " + spelled + "(0)");
		}

		if (expression.arguments.size() > 1) {
			return emitConstructList(expression, toType);
		}

		const Id value = emitExpression(*expression.arguments.front());
		const Id fromType = _builder.typeOf(value);

		if (_types.vectorWidth(fromType) == 1 && _types.vectorWidth(toType) > 1) {
			return broadcast(value, toType);
		}

		return convert(value, fromType, toType);
	}

	// float4(v3, 1), float4(v2, v2), float3(f, v2): the pieces fill the components
	// in the order written. Apple's compiler converts a scalar piece but takes a
	// vector piece only of the target's own component type, and the widths have to
	// add up to the target's; mslc accepts the same and rejects the rest.
	Id Emitter::emitConstructList(const Expression& expression, Id toType) {
		const std::string spelled = typeName(*expression.constructType);
		const Id component = _types.componentOf(toType);
		if (component == InvalidId) {
			throw CompileError(spelled + " is built from one value, and this passes "
				+ std::to_string(expression.arguments.size()));
		}

		std::vector<uint32_t> pieces;
		uint32_t components = 0;
		for (const ExpressionPtr& argument: expression.arguments) {
			const Id value = emitExpression(*argument);
			const Id type = _builder.typeOf(value);

			if (_types.componentOf(type) != InvalidId) {
				if (_types.componentOf(type) != component) {
					throw CompileError("a vector piece of " + spelled + " has to have its "
						"component type; only a scalar piece is converted");
				}
				pieces.push_back(value);
				components += _types.vectorWidth(type);
				continue;
			}

			if (_types.isAggregate(type)) {
				throw CompileError(spelled + " is built from scalars and vectors, not from "
					"a matrix or a struct");
			}

			pieces.push_back(convert(value, type, component));
			components += 1;
		}

		const uint32_t width = _types.vectorWidth(toType);
		if (components != width) {
			throw CompileError(spelled + " built from " + std::to_string(components)
				+ " components needs " + std::to_string(width));
		}

		return _builder.emitTyped(spirv::OpCompositeConstruct, toType, pieces);
	}

	Id Emitter::emitUnary(const Expression& expression) {
		const Id operand = emitExpression(*expression.left);
		const Id type = _builder.typeOf(operand);

		// Apple rejects every unary operator on a matrix, unary plus included, and
		// OpFNegate takes no matrix.
		if (_types.matrixInfo(type)) {
			throw CompileError("a unary operator on a matrix is not lowered yet");
		}

		if (_types.isBool(type) && expression.unaryOperator != UnaryOperator::Not) {
			throw CompileError("a unary operator on a bool is not lowered yet; "
				"convert the bool to an int first");
		}

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

		if (_types.matrixInfo(leftType) || _types.matrixInfo(_builder.typeOf(right))) {
			return emitMatrixProduct(expression.binaryOperator, left, right);
		}

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
			// promotion changes the answer. A uint beside an int needs no promotion:
			// the rank rule below leaves the wider type alone.
			//
			// Each operand is promoted on its own, and only the *types* are changed
			// here: the conversions are emitted below, once, when both operands go to
			// the common type. Promoting both sides to one int here instead of each
			// on its own would replace the conversions rather than precede them, and
			// then "char c = -1; uint u = 0; c < u" would compare two ints and answer
			// true where MSL makes both sides unsigned and answers false. It would
			// also truncate a 64-bit operand, which the conversions never do.
			if (!_types.isFloat(leftType) && !_types.isFloat(rightType)) {
				if (_types.bitWidth(leftType) < 32) {
					leftType = promotedTo(_types.vectorWidth(leftType));
				}
				if (_types.bitWidth(rightType) < 32) {
					rightType = promotedTo(_types.vectorWidth(rightType));
				}
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
				// The wider type wins outright, signedness and all. A long beside a
				// uint is a signed comparison, because long has the higher rank and can
				// represent every uint; making both sides unsigned here is what turned
				// that pair wrong.
				commonType = rightType;
				operandsSigned = _types.isSignedInt(rightType);
			} else if (_types.bitWidth(leftType) > _types.bitWidth(rightType)) {
				// The left is already the wider, which the default above already says.
			} else if (leftType != rightType) {
				// Equal widths and opposite signedness, which is the only case signedness
				// decides: C makes both sides unsigned.
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

		// Apple promotes a bool operand to int. mslc has no such promotion, and
		// OpIAdd on a bool is a module spirv-val rejects.
		if (_types.isBool(leftType) || _types.isBool(_builder.typeOf(right))) {
			throw CompileError("an arithmetic or bitwise operator on a bool is not lowered yet; "
				"convert the bool to an int first");
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

	// A product with a matrix on at least one side. SPIR-V has an opcode per
	// shape, and each needs its operands to agree in component type and in the
	// dimension the product runs over, so anything else is reported.
	Id Emitter::emitMatrixProduct(BinaryOperator op, Id left, Id right) {
		if (op != BinaryOperator::Multiply) {
			throw CompileError("only * is lowered with a matrix operand; any other operator on a "
				"matrix is not lowered yet");
		}

		const Id leftType = _builder.typeOf(left);
		const Id rightType = _builder.typeOf(right);
		const auto* leftMatrix = _types.matrixInfo(leftType);
		const auto* rightMatrix = _types.matrixInfo(rightType);
		const std::string mismatch = "the operands of a matrix product do not match: the "
			"columns of the left have to equal the rows of the right, in the same component type";

		const std::string scaleMismatch = "a matrix is scaled by a scalar of its own component "
			"type, and this one is another type";

		// The scalar has to be the matrix's own component type already: Apple's
		// compiler rejects a float4x4 times an int or a half rather than converting.
		if (leftMatrix && _types.vectorWidth(rightType) == 1 && !rightMatrix) {
			if (rightType != _types.scalar(leftMatrix->scalar)) {
				throw CompileError(scaleMismatch);
			}
			return _builder.emitTyped(spirv::OpMatrixTimesScalar, leftType, { left, right });
		}

		if (rightMatrix && _types.vectorWidth(leftType) == 1 && !leftMatrix) {
			if (leftType != _types.scalar(rightMatrix->scalar)) {
				throw CompileError(scaleMismatch);
			}
			return _builder.emitTyped(spirv::OpMatrixTimesScalar, rightType, { right, left });
		}

		if (leftMatrix && rightMatrix) {
			if (leftMatrix->scalar != rightMatrix->scalar || leftMatrix->columns != rightMatrix->rows) {
				throw CompileError(mismatch);
			}

			return _builder.emitTyped(spirv::OpMatrixTimesMatrix,
				_types.matrix(leftMatrix->scalar, rightMatrix->columns, leftMatrix->rows),
				{ left, right });
		}

		if (leftMatrix) {
			if (_types.componentOf(rightType) != _types.scalar(leftMatrix->scalar)
				|| _types.vectorWidth(rightType) != leftMatrix->columns) {
				throw CompileError(mismatch);
			}

			return _builder.emitTyped(spirv::OpMatrixTimesVector, leftMatrix->column, { left, right });
		}

		if (_types.componentOf(leftType) != _types.scalar(rightMatrix->scalar)
			|| _types.vectorWidth(leftType) != rightMatrix->rows) {
			throw CompileError(mismatch);
		}

		return _builder.emitTyped(spirv::OpVectorTimesMatrix,
			_types.vector(rightMatrix->scalar, rightMatrix->columns), { left, right });
	}

	Id Emitter::emitCall(const Expression& expression) {
		if (expression.left->kind == ExpressionKind::Member) {
			return emitTextureCall(expression);
		}

		if (expression.left->kind != ExpressionKind::Identifier) {
			throw CompileError("only a direct function call is supported");
		}

		// The parser takes no function but an entry point, so a call cannot name a
		// user function a builtin would shadow.
		const MathBuiltin* builtin = findMathBuiltin(expression.left->name);
		if (!builtin) {
			throw CompileError("function \"" + expression.left->name + "\" is not a builtin mslc "
				"recognises, and user functions are not lowered yet");
		}

		return emitMathBuiltin(*builtin, expression.arguments);
	}

	Id Emitter::emitMathBuiltin(const MathBuiltin& builtin,
		const std::vector<ExpressionPtr>& arguments) {

		const std::string name = builtin.name;
		if (arguments.size() != builtin.arity) {
			throw CompileError(name + " takes " + std::to_string(builtin.arity) + " argument"
				+ (builtin.arity == 1 ? "" : "s") + ", and this call passes "
				+ std::to_string(arguments.size()));
		}

		std::vector<Id> values;
		for (const ExpressionPtr& argument: arguments) {
			const Id value = emitExpression(*argument);
			const Id type = _builder.typeOf(value);
			if (_types.matrixInfo(type)) {
				throw CompileError(name + " takes scalars or vectors, not a matrix");
			}
			// A bool is one bit wide and a struct or pointer has no width at all.
			if (!_types.isFloat(type) && _types.bitWidth(type) < 8) {
				throw CompileError(name + " takes numeric scalars or vectors");
			}
			values.push_back(value);
		}

		// refract's eta is a scalar of its own, so only the first two of its
		// arguments say what type the call is in.
		const size_t sharing = builtin.shape == MathShape::Refract ? 2 : values.size();

		const bool takesIntegers = builtin.signedInstruction != kNoInstruction;

		// The type the call is in: the first vector argument's, since a scalar
		// beside it is broadcast. Among scalars alone it is the first float
		// argument's, as Apple types pow(x, 2) as float, or else the first's.
		Id type = _builder.typeOf(values[0]);
		for (size_t i = 0; i < sharing; ++i) {
			const Id argumentType = _builder.typeOf(values[i]);
			if (_types.vectorWidth(argumentType) > 1) {
				type = argumentType;
				break;
			}
			if (!_types.isFloat(type) && _types.isFloat(argumentType)) {
				type = argumentType;
			}
		}

		const uint32_t width = _types.vectorWidth(type);
		const Id component = width > 1 ? _types.componentOf(type) : type;
		const bool isFloat = _types.isFloat(type);
		const bool isSigned = _types.isSignedInt(type);
		const uint32_t bits = _types.bitWidth(type);

		const bool takesScalars = builtin.shape == MathShape::Componentwise
			|| builtin.shape == MathShape::Saturate;
		// Metal has no double, and GLSL.std.450's transcendentals take only 16 and
		// 32 bits.
		if (isFloat ? bits > 32 : !takesIntegers) {
			throw CompileError(name + " takes float or half" + (takesIntegers ? " or integer" : "")
				+ (takesScalars ? " scalars or vectors" : " vectors"));
		}

		if (!takesScalars && width == 1) {
			throw CompileError(name + " takes vectors, and Metal has no scalar " + name);
		}

		if (builtin.shape == MathShape::Cross && width != 3) {
			throw CompileError("cross takes three-component vectors, and these have "
				+ std::to_string(width));
		}

		// A scalar beside a vector is broadcast, as Metal converts a scalar to any
		// vector. Beside a float scalar only an integer is converted, and only
		// for a builtin with no integer form: Apple reports min(int, float) and
		// mix(half, half, float) as ambiguous.
		for (size_t i = 0; i < sharing; ++i) {
			const Id argumentType = _builder.typeOf(values[i]);
			if (argumentType == type) {
				continue;
			}

			const bool converts = width == 1 && !takesIntegers && !_types.isFloat(argumentType);
			if (_types.vectorWidth(argumentType) != 1 || (width == 1 && !converts)) {
				throw CompileError("the arguments of " + name + " have to be one type, or a "
					"scalar beside a vector");
			}

			values[i] = width > 1 ? broadcast(values[i], type) : convert(values[i], argumentType, type);
		}

		if (builtin.shape == MathShape::Refract) {
			if (_types.vectorWidth(_builder.typeOf(values[2])) != 1) {
				throw CompileError("refract's third argument is the scalar ratio of the indices "
					"of refraction, not a vector");
			}
			values[2] = convert(values[2], _builder.typeOf(values[2]), component);
		}

		if (builtin.shape == MathShape::Dot) {
			return _builder.emitTyped(spirv::OpDot, component, { values[0], values[1] });
		}

		if (builtin.shape == MathShape::Saturate) {
			Id zero = _builder.emitDeclTyped(spirv::OpConstant, component, { 0u });
			// 1.0 as a half is 0x3C00, not the float's bits; 0.0 is zero bits in both.
			Id one = _builder.emitDeclTyped(spirv::OpConstant, component,
				{ bits == 16 ? 0x3C00u : 0x3F800000u });
			if (width > 1) {
				zero = _builder.emitDeclTyped(spirv::OpConstantComposite, type,
					std::vector<uint32_t>(width, zero));
				one = _builder.emitDeclTyped(spirv::OpConstantComposite, type,
					std::vector<uint32_t>(width, one));
			}
			values.push_back(zero);
			values.push_back(one);
		}

		const uint32_t instruction = isFloat ? builtin.floatInstruction
			: isSigned ? builtin.signedInstruction : builtin.unsignedInstruction;
		if (instruction == kNoInstruction) {
			throw CompileError(name + " of an unsigned integer is not lowered yet");
		}

		if (_glslSet == InvalidId) {
			// The set name is a literal string operand, not an OpString id.
			std::vector<uint32_t> setName;
			spirv::Builder::appendString(setName, "GLSL.std.450");
			_glslSet = _builder.emitDecl(spirv::OpExtInstImport, setName);
		}

		const Id result = builtin.shape == MathShape::VectorToScalar ? component : type;
		std::vector<uint32_t> operands { _glslSet, instruction };
		operands.insert(operands.end(), values.begin(), values.end());
		return _builder.emitTyped(spirv::OpExtInst, result, operands);
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
		if (type.isMatrix()) {
			throw CompileError("a " + typeName(type) + " constant is not lowered yet");
		}

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
				|| parameter.attributes.samplerIndex || parameter.attributes.builtin
				|| parameter.attributes.stageIn || parameter.type.resource != ResourceKind::None) {
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
		std::vector<std::pair<size_t, const Parameter*>> resources;

		for (size_t index = 0; index < _entryPoint->parameters.size(); ++index) {
			const Parameter& parameter = _entryPoint->parameters[index];
			const ParameterAttributes& attributes = parameter.attributes;

			// A texture or sampler parameter, or an attribute that says it is one.
			// Apple takes [[texture]] on a texture and [[sampler]] on a sampler and
			// nothing else: "type 'device float *' is not valid for attribute
			// 'texture'".
			const bool isTexture = parameter.type.resource == ResourceKind::Texture2D;
			const bool isSampler = parameter.type.resource == ResourceKind::Sampler;
			if (isTexture || isSampler || attributes.textureIndex || attributes.samplerIndex) {
				const char* expected = isTexture ? "texture" : "sampler";
				if ((attributes.textureIndex && !isTexture) || (attributes.samplerIndex && !isSampler)
					|| attributes.bufferIndex || attributes.builtin || attributes.stageIn) {
					throw CompileError("parameter \"" + parameter.name + "\" is a " + typeName(parameter.type)
						+ ", and its attribute is not valid for it"
						+ (isTexture || isSampler ? std::string("; it takes [[") + expected + "(n)]] only" : ""));
				}

				resources.emplace_back(index, &parameter);
				continue;
			}

			if (parameter.attributes.stageIn) {
				if (_entryPoint->stage != Stage::Fragment) {
					throw CompileError("[[stage_in]] on a "
						+ std::string(_entryPoint->stage == Stage::Vertex ? "vertex" : "kernel")
						+ " function is not lowered yet (parameter \"" + parameter.name
						+ "\"); mslc lowers it on a fragment function only");
				}

				if (_stageIn) {
					throw CompileError("a fragment function takes one [[stage_in]] parameter, and \""
						+ _entryPoint->name + "\" has a second (\"" + parameter.name + "\")");
				}

				const StructDecl* decl = structValue(parameter.type);
				if (!decl) {
					throw CompileError("[[stage_in]] parameter \"" + parameter.name + "\" is a "
						+ typeName(parameter.type) + "; mslc lowers a [[stage_in]] struct and no "
							"other type");
				}

				_stageIn = &parameter;
				_stageInputs = declareStageStruct(*decl, spirv::StorageClass::Input);
				continue;
			}

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
			// A matrix's layout is a MatrixStride, which SPIR-V only allows on a
			// struct member, and a buffer reached directly is not wrapped in one.
			if (parameter.type.isMatrix() && !parameter.type.isPointer) {
				throw CompileError("parameter \"" + parameter.name + "\" is a " + typeName(parameter.type)
					+ " reached by reference, which is not lowered yet; pass a pointer to it, "
						"or put it in a struct");
			}

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
			bufferPointee = _types.blockStructFor(pointeeType, parameter.type.isPacked);
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
		binding.readOnly = parameter.type.isConst
			|| parameter.type.addressSpace == AddressSpace::Constant;
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
		addReflectionEntry("{ \"kind\": \"Buffer\", \"metal_index\": "
			+ std::to_string(bindingIndex)
			+ ", \"descriptor\": { \"set\": " + std::to_string(descriptorSet())
			+ ", \"binding\": 0 }"
			+ ", \"member\": " + std::to_string(memberIndex)
			+ ", \"param_index\": " + std::to_string(index)
			+ ", \"name\": \"" + parameter.name + "\" }");
	}

		// One block for every buffer parameter, at binding 0. It is emitted here
		// rather than per parameter because its member list is only known once the
		// loop is done, and indium binds it as a single uniform buffer whose
		// entries are the buffer addresses in this order.
		if (!_bufferMembers.empty()) {
			_addressBlock = _types.addressBlock(_bufferMembers, descriptorSet());
			_interface.push_back(_addressBlock.variable);
		}

		declareResources(resources);
		std::set<std::string> used;
		collectIdentifiers(*_entryPoint->body, used);
		reserveLocalSamplers(*_entryPoint->body, used);
	}

	// One entry per binding goes in the reflection. The separator goes before the
	// entry rather than after it, because a trailing comma makes the array a
	// syntax error: json.load refuses "Illegal trailing comma before end of
	// array", so a reflection a reader cannot parse is not a reflection.
	void Emitter::addReflectionEntry(const std::string& entry) {
		if (!_reflection.empty()) {
			_reflection.pop_back();  // the newline the previous entry ended with
			_reflection += ",\n";
		}

		_reflection += "\t\t\t\t" + entry + "\n";
	}

	std::string Emitter::descriptorJson(uint32_t binding) const {
		return "{ \"set\": " + std::to_string(descriptorSet()) + ", \"binding\": "
			+ std::to_string(binding) + " }";
	}

	// A UniformConstant variable of the pointee's type at the next descriptor
	// binding of this entry point's set.
	Id Emitter::declareDescriptorVariable(Id pointee, uint32_t binding) {
		const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::UniformConstant, pointee),
			{ static_cast<uint32_t>(spirv::StorageClass::UniformConstant) });
		_builder.emit(spirv::OpDecorate, { variable,
			static_cast<uint32_t>(spirv::Decoration::DescriptorSet), descriptorSet() });
		_builder.emit(spirv::OpDecorate, { variable,
			static_cast<uint32_t>(spirv::Decoration::Binding), binding });
		_interface.push_back(variable);
		return variable;
	}

	// Textures take the bindings after the address block, in declaration order,
	// then the samplers, then the constexpr samplers as the body declares them.
	// That is Iridium's order (indium src/iridium/air.cpp: the texture loop, the
	// sampler loop, then air.sampler_states), and it is what indium binds by.
	// The index among its own kind is the Metal index, and an unattributed
	// parameter takes the smallest one no attribute claims, as Apple's does.
	void Emitter::declareResources(const std::vector<std::pair<size_t, const Parameter*>>& resources) {
		_nextBinding = _bufferMembers.empty() ? 0u : 1u;

		struct Entry {
			size_t index;
			const Parameter* parameter;
			uint32_t metalIndex;
		};
		std::vector<Entry> textures;
		std::vector<Entry> samplers;
		// An unattributed parameter takes the smallest index of its kind that no
		// attribute anywhere in the list claims and no earlier parameter took.
		std::set<uint32_t> taken[2];
		for (const auto& [index, parameter]: resources) {
			(void)index;
			const bool isTexture = parameter->type.resource == ResourceKind::Texture2D;
			const auto& declared = isTexture ? parameter->attributes.textureIndex
				: parameter->attributes.samplerIndex;
			if (declared) {
				taken[isTexture ? 0 : 1].insert(*declared);
			}
		}

		for (const auto& [index, parameter]: resources) {
			const bool isTexture = parameter->type.resource == ResourceKind::Texture2D;
			const auto& declared = isTexture ? parameter->attributes.textureIndex
				: parameter->attributes.samplerIndex;
			uint32_t metalIndex = 0;
			if (declared) {
				metalIndex = *declared;
			} else {
				std::set<uint32_t>& claimed = taken[isTexture ? 0 : 1];
				while (claimed.count(metalIndex)) {
					++metalIndex;
				}
				const uint32_t limit = isTexture ? 127 : 15;
				if (metalIndex > limit) {
					throw CompileError(std::string("no '") + (isTexture ? "texture" : "sampler")
						+ "' resource location is available for parameter \"" + parameter->name
						+ "\": every index up to " + std::to_string(limit) + " is taken");
				}
				claimed.insert(metalIndex);
			}
			(isTexture ? textures : samplers).push_back({ index, parameter, metalIndex });
		}

		for (const Entry& entry: textures) {
			const uint32_t binding = _nextBinding++;
			const Id variable = declareDescriptorVariable(_types.image2D(), binding);

			Binding bound;
			bound.id = variable;
			bound.isPointer = true;
			bound.pointeeType = _types.image2D();
			bound.storageClass = spirv::StorageClass::UniformConstant;
			bound.pointeeMsl = entry.parameter->type;
			_bindings[entry.parameter->name] = bound;

			addReflectionEntry("{ \"kind\": \"Texture\", \"metal_index\": "
				+ std::to_string(entry.metalIndex)
				+ ", \"descriptor\": " + descriptorJson(binding)
				+ ", \"texture_access\": \"Sample\""
				+ ", \"param_index\": " + std::to_string(entry.index)
				+ ", \"name\": \"" + entry.parameter->name + "\" }");
		}

		for (const Entry& entry: samplers) {
			const uint32_t binding = _nextBinding++;
			const Id variable = declareDescriptorVariable(_types.samplerType(), binding);

			Binding bound;
			bound.id = variable;
			bound.isPointer = true;
			bound.pointeeType = _types.samplerType();
			bound.storageClass = spirv::StorageClass::UniformConstant;
			bound.pointeeMsl = entry.parameter->type;
			_bindings[entry.parameter->name] = bound;

			addReflectionEntry("{ \"kind\": \"Sampler\", \"metal_index\": "
				+ std::to_string(entry.metalIndex)
				+ ", \"descriptor\": " + descriptorJson(binding)
				+ ", \"param_index\": " + std::to_string(entry.index)
				+ ", \"name\": \"" + entry.parameter->name + "\" }");
		}
	}

	// A sampler declared in the shader is a sampler variable at the next binding,
	// and the reflection carries its state, because there is nothing for the app
	// to bind: indium creates the immutable sampler from it, as it does from
	// Iridium's EmbeddedSampler. Equal states share one variable and one binding.
	//
	// Every one in the body is reserved before the entry point is emitted, since
	// the variable has to be in the OpEntryPoint interface, which comes first.
	void Emitter::reserveLocalSamplers(const Statement& statement, const std::set<std::string>& used) {
		if (statement.declaration && statement.declaration->sampler
			&& used.count(statement.declaration->name)) {
			const VariableDeclaration& declaration = *statement.declaration;
			const bool known = std::any_of(_embeddedSamplers.begin(), _embeddedSamplers.end(),
				[&](const EmbeddedSampler& other) { return other.state == *declaration.sampler; });
			if (!known) {
				const uint32_t binding = _nextBinding++;
				_embeddedSamplers.push_back({ *declaration.sampler,
					declareDescriptorVariable(_types.samplerType(), binding) });

				addReflectionEntry("{ \"kind\": \"Sampler\", \"descriptor\": " + descriptorJson(binding)
					+ ", \"embedded_sampler\": " + std::to_string(_embeddedSamplers.size() - 1)
					+ ", \"name\": \"" + declaration.name + "\" }");
			}
		}

		for (const StatementPtr& child: statement.children) {
			reserveLocalSamplers(*child, used);
		}
		for (const Statement* nested: { statement.thenBranch.get(), statement.elseBranch.get(),
			statement.forBody.get(), statement.whileBody.get() }) {
			if (nested) {
				reserveLocalSamplers(*nested, used);
			}
		}
	}

	void Emitter::declareLocalSampler(const VariableDeclaration& declaration) {
		const auto found = std::find_if(_embeddedSamplers.begin(), _embeddedSamplers.end(),
			[&](const EmbeddedSampler& other) { return other.state == *declaration.sampler; });
		if (found == _embeddedSamplers.end()) {
			return;  // never named after its declaration, so Apple records no state for it
		}

		Binding bound;
		bound.id = found->variable;
		bound.isPointer = true;
		bound.pointeeType = _types.samplerType();
		bound.storageClass = spirv::StorageClass::UniformConstant;
		bound.pointeeMsl = declaration.type;
		bound.unnormalizedSampler = !declaration.sampler->normalizedCoordinates;
		_bindings[declaration.name] = bound;
	}

	// A half texture is sampled as a float image and narrowed here, because the
	// image type has a float sampled type whatever the texture's component is.
	Id Emitter::texturePixels(const Binding& texture, Id sampled) {
		if (texture.pointeeMsl.scalar != ScalarKind::Half) {
			return sampled;
		}

		return convert(sampled, _builder.typeOf(sampled), _types.vector(ScalarKind::Half, 4));
	}

	Id Emitter::emitTextureCall(const Expression& call) {
		const Expression& member = *call.left;
		if (member.left->kind != ExpressionKind::Identifier) {
			throw CompileError("the receiver of \"." + member.memberName + "\" has to be a texture parameter");
		}

		const auto it = _bindings.find(member.left->name);
		if (it == _bindings.end() || it->second.pointeeMsl.resource != ResourceKind::Texture2D) {
			throw CompileError("\"" + member.left->name + "\" is not a texture parameter, so "
				"\"." + member.memberName + "\" is not a call mslc lowers");
		}

		const std::string& method = member.memberName;
		if (method == "sample") {
			return emitTextureSample(call, it->second);
		}
		if (method == "read") {
			return emitTextureRead(call, it->second);
		}
		if (method == "get_width" || method == "get_height") {
			return emitTextureSize(call, it->second, method == "get_width");
		}

		throw CompileError("the texture method \"" + method + "\" is not lowered yet; mslc lowers "
			"sample, read, get_width and get_height");
	}

	// sample(sampler, float2 coordinate [, level(lod)]). A fragment function takes
	// the level from the derivatives, which is OpImageSampleImplicitLod; a vertex
	// or kernel function has none, so it samples level 0 with an explicit lod,
	// and so does a sampler with pixel coordinates, which Vulkan does not allow
	// an implicit lod on.
	Id Emitter::emitTextureSample(const Expression& call, const Binding& texture) {
		const auto& arguments = call.arguments;
		if (arguments.size() < 2 || arguments.size() > 3) {
			throw CompileError("sample takes a sampler, a coordinate and optionally level(lod); "
				"this call passes " + std::to_string(arguments.size()));
		}

		if (arguments[0]->kind != ExpressionKind::Identifier) {
			throw CompileError("the first argument of sample has to name a sampler");
		}
		const auto samplerIt = _bindings.find(arguments[0]->name);
		if (samplerIt == _bindings.end() || samplerIt->second.pointeeMsl.resource != ResourceKind::Sampler) {
			throw CompileError("\"" + arguments[0]->name + "\" is not a sampler, and sample takes one "
				"as its first argument");
		}
		const Binding& sampler = samplerIt->second;

		const Id float2 = _types.vector(ScalarKind::Float, 2);
		Id coordinate = emitExpression(*arguments[1]);
		const Id coordinateType = _builder.typeOf(coordinate);
		if (coordinateType != float2) {
			if (_types.vectorWidth(coordinateType) != 1 || _types.bitWidth(coordinateType) < 8) {
				throw CompileError("the coordinate of sample has to be a float2 or a number");
			}
			coordinate = broadcast(coordinate, float2);
		}

		Id lod = InvalidId;
		if (arguments.size() == 3) {
			const Expression& option = *arguments[2];
			const bool isCall = option.kind == ExpressionKind::Call
				&& option.left->kind == ExpressionKind::Identifier;
			if (isCall && option.left->name == "level") {
				if (option.arguments.size() != 1) {
					throw CompileError("level takes one argument");
				}
				const Id value = emitExpression(*option.arguments[0]);
				const Id valueType = _builder.typeOf(value);
				if (_types.vectorWidth(valueType) != 1 || _types.bitWidth(valueType) < 8) {
					throw CompileError("level takes a scalar number");
				}
				lod = convert(value, valueType, _types.scalar(ScalarKind::Float));
			} else if (isCall && (option.left->name == "bias" || option.left->name == "gradient2d"
				|| option.left->name == "min_lod_clamp")) {
				throw CompileError("the sample option " + option.left->name + "(...) is not lowered "
					"yet; mslc lowers level(lod)");
			} else {
				throw CompileError("the third argument of sample has to be level(lod); an offset "
					"and the other options are not lowered yet");
			}
		}

		// Vulkan requires Lod 0 through an unnormalized sampler, whatever level() says.
		if (sampler.unnormalizedSampler) {
			lod = _builder.emitDeclTyped(spirv::OpConstant, _types.scalar(ScalarKind::Float), { 0u });
		}

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id loadedSampler = loadFrom(sampler.id, sampler.pointeeType);
		const Id combined = _builder.emitTyped(spirv::OpSampledImage, _types.sampledImage2D(),
			{ image, loadedSampler });

		const Id float4 = _types.vector(ScalarKind::Float, 4);
		Id sampled = InvalidId;
		if (lod == InvalidId && _entryPoint->stage == Stage::Fragment && !sampler.unnormalizedSampler) {
			sampled = _builder.emitTyped(spirv::OpImageSampleImplicitLod, float4, { combined, coordinate });
		} else {
			if (lod == InvalidId) {
				lod = _builder.emitDeclTyped(spirv::OpConstant, _types.scalar(ScalarKind::Float), { 0u });
			}
			sampled = _builder.emitTyped(spirv::OpImageSampleExplicitLod, float4,
				{ combined, coordinate, kImageOperandsLod, lod });
		}

		return texturePixels(texture, sampled);
	}

	// read(uint2 coordinate [, lod]): a texel by integer coordinate, with no
	// sampler. Metal takes uint2 and ushort2 and rejects int2 and float2.
	Id Emitter::emitTextureRead(const Expression& call, const Binding& texture) {
		const auto& arguments = call.arguments;
		if (arguments.empty() || arguments.size() > 2) {
			throw CompileError("read takes a coordinate and optionally a lod; this call passes "
				+ std::to_string(arguments.size()));
		}

		Id coordinate = emitExpression(*arguments[0]);
		const Id coordinateType = _builder.typeOf(coordinate);
		const bool unsignedPair = _types.vectorWidth(coordinateType) == 2
			&& !_types.isFloat(coordinateType) && !_types.isSignedInt(coordinateType)
			&& (_types.bitWidth(coordinateType) == 32 || _types.bitWidth(coordinateType) == 16);
		if (!unsignedPair) {
			throw CompileError("the coordinate of read has to be a uint2 or a ushort2");
		}
		coordinate = convert(coordinate, coordinateType, _types.vector(ScalarKind::UInt, 2));

		Id lod = arguments.size() == 2 ? emitLod(*arguments[1], "read") : constantU32(0);

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id fetched = _builder.emitTyped(spirv::OpImageFetch,
			_types.vector(ScalarKind::Float, 4), { image, coordinate, kImageOperandsLod, lod });
		return texturePixels(texture, fetched);
	}

	// get_width() and get_height(), of level 0 or of the lod given.
	Id Emitter::emitTextureSize(const Expression& call, const Binding& texture, bool width) {
		const std::string name = width ? "get_width" : "get_height";
		if (call.arguments.size() > 1) {
			throw CompileError(name + " takes an optional lod; this call passes "
				+ std::to_string(call.arguments.size()));
		}

		const Id lod = call.arguments.empty() ? constantU32(0) : emitLod(*call.arguments[0], name);

		if (!_imageQueryDeclared) {
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageQuery) });
			_imageQueryDeclared = true;
		}

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id size = _builder.emitTyped(spirv::OpImageQuerySizeLod,
			_types.vector(ScalarKind::UInt, 2), { image, lod });
		return _builder.emitTyped(spirv::OpCompositeExtract, _uintType, { size, width ? 0u : 1u });
	}

	// A mip level argument: any number, converted to the uint the instruction takes.
	Id Emitter::emitLod(const Expression& expression, const std::string& callName) {
		const Id value = emitExpression(expression);
		const Id type = _builder.typeOf(value);
		if (_types.vectorWidth(type) != 1 || _types.bitWidth(type) < 8) {
			throw CompileError("the lod of " + callName + " has to be a number");
		}

		return convert(value, type, _uintType);
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

	// The struct a type names when it is a struct value, and not a pointer to one
	// or an array of them.
	const StructDecl* Emitter::structValue(const Type& type) const {
		return type.isPointer || type.arrayLength ? nullptr : _unit.findStruct(type.namedType);
	}

	// A half crosses as a float: Vulkan's shaderFloat16 does not cover the Input
	// and Output storage classes, which need storageInputOutput16, a feature a
	// device need not have, and widening a half is exact. Narrower integers,
	// 64-bit values, bools and anything wider than a vector are reported rather
	// than given a layout guessed at.
	StageVariable Emitter::declareStageVariable(const Type& type,
		spirv::StorageClassValue storageClass, const std::string& what) {

		if (type.isPacked) {
			throw CompileError(what + " is a " + typeName(type) + ", which Apple's compiler does not "
				"allow across a stage boundary; use " + typeName(Type { type.scalar, type.vectorWidth }));
		}

		const uint32_t bits = scalarBitWidth(type.scalar);
		if (type.isPointer || type.arrayLength || !type.namedType.empty() || type.isMatrix()
			|| type.scalar == ScalarKind::Bool || (bits != 32 && type.scalar != ScalarKind::Half)) {
			throw CompileError(what + " is a " + typeName(type) + ", which mslc does not pass "
				"between stages yet; it passes a scalar or a vector of 32-bit components or of half");
		}

		Type carried = type;
		if (carried.scalar == ScalarKind::Half) {
			carried.scalar = ScalarKind::Float;
		}

		StageVariable stage;
		stage.valueType = declaredTypeOf(type);
		stage.interfaceType = declaredTypeOf(carried);
		stage.variable = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(storageClass, stage.interfaceType), { static_cast<uint32_t>(storageClass) });
		_interface.push_back(stage.variable);
		return stage;
	}

	// One variable per field. The [[position]] field is a builtin, Position on a
	// vertex output and FragCoord on a fragment input; every other field takes the
	// next Location in declaration order, which is how Iridium numbers them on
	// both sides (indium src/iridium/air.cpp: the struct return loop and the
	// air.fragment_input case), so the two stages agree when they share a struct.
	std::vector<StageVariable> Emitter::declareStageStruct(const StructDecl& decl,
		spirv::StorageClassValue storageClass) {

		const bool isInput = storageClass == spirv::StorageClass::Input;
		std::vector<StageVariable> variables;
		uint32_t location = 0;
		bool hasPosition = false;

		for (const StructField& field: decl.fields) {
			const std::string what = "field \"" + field.name + "\" of \"" + decl.name + "\"";

			if (field.attributes.attributeIndex) {
				throw CompileError(what + " has [[attribute(n)]], which mslc does not lower "
					"on a struct crossing from the vertex to the fragment stage");
			}

			const StageVariable variable = declareStageVariable(field.type, storageClass, what);

			if (field.attributes.position) {
				if (hasPosition) {
					throw CompileError("struct \"" + decl.name + "\" has more than one [[position]] field");
				}
				if (variable.valueType != _types.vector(ScalarKind::Float, 4)) {
					throw CompileError(what + " is [[position]], which has to be a float4");
				}

				hasPosition = true;
				_builder.emit(spirv::OpDecorate, { variable.variable,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(isInput ? spirv::BuiltIn::FragCoord : spirv::BuiltIn::Position) });
			} else {
				_builder.emit(spirv::OpDecorate, { variable.variable,
					static_cast<uint32_t>(spirv::Decoration::Location), location++ });

				// VUID-StandaloneSpirv-Flat-04744: an integer fragment input is not
				// interpolated, and the module is rejected unless it says so.
				if (isInput && !_types.isFloat(variable.interfaceType)) {
					_builder.emit(spirv::OpDecorate, { variable.variable,
						static_cast<uint32_t>(spirv::Decoration::Flat) });
				}
			}

			variables.push_back(variable);
		}

		// Apple rejects a vertex function whose returned struct has no position
		// ("invalid return type"), and a rasteriser has nothing to place without it.
		if (!isInput && !hasPosition) {
			throw CompileError("struct \"" + decl.name + "\" is returned by a vertex function and "
				"has no [[position]] field");
		}

		return variables;
	}

	// The variables a return statement writes. A vertex function returns a struct
	// with a [[position]] field, and a fragment function one colour, at Location 0.
	void Emitter::declareStageOutputs() {
		const Type& type = _entryPoint->returnType;
		if (type.namedType.empty() && !type.isPointer && type.scalar == ScalarKind::Void) {
			return;
		}

		const std::string spelled = typeName(type);
		if (_entryPoint->stage == Stage::Kernel) {
			throw CompileError("a kernel returns void, and \"" + _entryPoint->name + "\" returns "
				+ spelled);
		}

		if (type.namedType.empty()) {
			if (_entryPoint->stage == Stage::Vertex) {
				throw CompileError("vertex function \"" + _entryPoint->name + "\" returns " + spelled
					+ " rather than a struct, which is not lowered yet");
			}

			_outputs.push_back(declareStageVariable(type, spirv::StorageClass::Output,
				"the value fragment function \"" + _entryPoint->name + "\" returns"));
			_builder.emit(spirv::OpDecorate, { _outputs.back().variable,
				static_cast<uint32_t>(spirv::Decoration::Location), 0u });
			return;
		}

		if (_entryPoint->stage == Stage::Fragment) {
			throw CompileError("fragment function \"" + _entryPoint->name + "\" returns the struct "
				+ spelled + ", and a struct of several colour attachments is not lowered yet");
		}

		const StructDecl* decl = structValue(type);
		if (!decl) {
			throw CompileError("vertex function \"" + _entryPoint->name + "\" returns " + spelled
				+ ", and mslc lowers a returned struct this source declares and nothing else");
		}

		_outputs = declareStageStruct(*decl, spirv::StorageClass::Output);
	}

	// The [[stage_in]] parameter becomes a local holding the struct, filled from
	// the Input variables at the top of the function, so a field of it is read
	// the way a field of any local is.
	void Emitter::loadStageInputs() {
		if (!_stageIn) {
			return;
		}

		const Id structType = declaredTypeOf(_stageIn->type);
		std::vector<uint32_t> fields;
		for (const StageVariable& input: _stageInputs) {
			const Id loaded = loadFrom(input.variable, input.interfaceType);
			fields.push_back(convert(loaded, input.interfaceType, input.valueType));
		}

		const Id local = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Function, structType),
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });
		_builder.emit(spirv::OpStore, { local,
			_builder.emitTyped(spirv::OpCompositeConstruct, structType, fields) });

		bindLocal(_stageIn->name, local, structType, _stageIn->type);
	}

	// Every entry point is a void function, so a returned value is written to
	// the Output variables and the function returns nothing.
	void Emitter::emitReturn(const Statement& statement) {
		if (!statement.expression) {
			if (!_outputs.empty()) {
				throw CompileError("\"" + _entryPoint->name + "\" returns "
					+ typeName(_entryPoint->returnType) + ", so a return needs a value");
			}

			terminate(spirv::OpReturn, { });
			return;
		}

		if (_outputs.empty()) {
			throw CompileError("\"" + _entryPoint->name + "\" returns void, so its return "
				"cannot carry a value");
		}

		const Id value = emitExpression(*statement.expression);
		const Id declared = declaredTypeOf(_entryPoint->returnType);

		if (!_entryPoint->returnType.namedType.empty()) {
			if (_builder.typeOf(value) != declared) {
				const std::string spelled = typeName(_entryPoint->returnType);
				throw CompileError("\"" + _entryPoint->name + "\" returns " + spelled + ", and mslc "
					"returns a local of that type; this return's value is another struct, or one "
					"read straight from a buffer, whose layout differs");
			}

			for (size_t i = 0; i < _outputs.size(); ++i) {
				const StageVariable& output = _outputs[i];
				const Id field = _builder.emitTyped(spirv::OpCompositeExtract, output.valueType,
					{ value, static_cast<uint32_t>(i) });
				_builder.emit(spirv::OpStore, { output.variable,
					convert(field, output.valueType, output.interfaceType) });
			}
		} else {
			const StageVariable& output = _outputs.front();
			const Id returned = convert(value, _builder.typeOf(value), declared);
			_builder.emit(spirv::OpStore, { output.variable,
				convert(returned, output.valueType, output.interfaceType) });
		}

		terminate(spirv::OpReturn, { });
	}

	// Locations are numbered by declaration order, where Apple pairs a vertex
	// output with a fragment input by field name and type. A vertex and a fragment
	// function pair when every field the fragment reads is one the vertex function
	// writes, and such a pair agrees only when the fragment's fields are a leading
	// run of the vertex function's. A fragment in another library cannot be checked.
	void Emitter::checkStageInterfacesAgree() const {
		const auto interfaceFields = [this](const std::string& name) {
			std::vector<std::pair<std::string, std::string>> fields;
			if (const StructDecl* decl = _unit.findStruct(name)) {
				for (const StructField& field: decl->fields) {
					if (!field.attributes.position) {
						fields.emplace_back(field.name, typeName(field.type));
					}
				}
			}
			return fields;
		};

		for (const FunctionDecl* vertex: _entryPoints) {
			if (vertex->stage != Stage::Vertex || vertex->returnType.namedType.empty()) {
				continue;
			}

			for (const FunctionDecl* fragment: _entryPoints) {
				if (fragment->stage != Stage::Fragment) {
					continue;
				}

				const auto outputs = interfaceFields(vertex->returnType.namedType);
				for (const Parameter& parameter: fragment->parameters) {
					if (!parameter.attributes.stageIn) {
						continue;
					}

					// Apple pairs by name and type, so a fragment reading a field this
					// vertex function does not write is not its pipeline partner.
					const auto inputs = interfaceFields(parameter.type.namedType);
					const bool pairs = std::all_of(inputs.begin(), inputs.end(), [&](const auto& input) {
						return std::find(outputs.begin(), outputs.end(), input) != outputs.end();
					});
					if (pairs && std::mismatch(inputs.begin(), inputs.end(),
						outputs.begin(), outputs.end()).first != inputs.end()) {
						throw CompileError("vertex function \"" + vertex->name + "\" returns "
							+ vertex->returnType.namedType + " and fragment function \"" + fragment->name
							+ "\" takes " + typeName(parameter.type) + " as [[stage_in]]; mslc gives "
							"their fields Locations by declaration order, so the fragment's fields, other "
							"than [[position]], have to be the vertex function's first ones, in order");
					}
				}
			}
		}
	}


	// A local variable becomes a Function-storage pointer, which the
	// expression path then loads from, so a local and a parameter behave the
	// same way when a name is used.
	void Emitter::emitVariableDeclaration(const VariableDeclaration& declaration) {
		if (declaration.type.resource != ResourceKind::None) {
			declareLocalSampler(declaration);
			return;
		}

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
			// convert() has no struct form and would emit an OpBitcast between them.
			if (!declaration.type.namedType.empty() && _builder.typeOf(initializer) != typeId) {
				throw CompileError("local \"" + declaration.name + "\" of type "
					+ typeName(declaration.type) + " is initialised from another struct, or from one "
					"read straight from a buffer, whose layout differs; mslc does not copy between "
					"them yet");
			}
			initial = convert(initializer, _builder.typeOf(initializer), typeId);
		} else {
			initial = _types.zero(typeId);
		}

		const Id pointerType = _types.pointer(spirv::StorageClass::Function, typeId);
		const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });
		_builder.emit(spirv::OpStore, { id, initial });

		bindLocal(declaration.name, id, typeId, declaration.type);
	}

	// pointeeMsl is what a member access on the local resolves its fields against.
	void Emitter::bindLocal(const std::string& name, Id variable, Id type, const Type& msl) {
		Binding binding;
		binding.id = variable;
		binding.isPointer = true;
		binding.pointeeType = type;
		binding.storageClass = spirv::StorageClass::Function;
		binding.pointeeMsl = msl;
		binding.readOnly = msl.isConst;
		_bindings[name] = binding;
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

	// The left side of an assignment is a place, so it is emitted as an address
	// rather than loaded.
	Id Emitter::emitPlaceAddress(const Expression& left) {
		Id address = InvalidId;
		if (left.kind == ExpressionKind::Index) {
			address = emitIndex(left, true);
		} else if (left.kind == ExpressionKind::Member) {
			Id fieldType = InvalidId;
			address = emitMemberAddress(left, fieldType);
		} else if (left.kind == ExpressionKind::Identifier) {
			// A local names a Function-storage pointer, so the variable itself is
			// the address to store to. A buffer parameter is a descriptor and a
			// plain value is loaded, so only this case takes the binding.
			const auto it = _bindings.find(left.name);
			if (it != _bindings.end() && it->second.isPointer
				&& it->second.storageClass == spirv::StorageClass::Function) {
				address = it->second.id;
			} else {
				address = emitExpression(left);
			}
		} else {
			address = emitExpression(left);
		}

		return address;
	}

	// v.xy = e, s.pos.zw = e, buf[i].w = e: the lanes the swizzle names are
	// replaced in the whole vector and the vector is stored back, so the lanes it
	// does not name keep their value. The address is computed once.
	void Emitter::emitSwizzleStore(const Expression& target, const Expression& valueExpression) {
		const std::string& name = target.memberName;
		const std::string quoted = "\"." + name + "\"";
		const std::vector<uint32_t> lanes = swizzleLanes(name);
		for (size_t i = 0; i < lanes.size(); ++i) {
			for (size_t j = i + 1; j < lanes.size(); ++j) {
				if (lanes[i] == lanes[j]) {
					throw CompileError("assigning to " + quoted + " names a component twice");
				}
			}
		}

		const Expression& base = *target.left;
		if (base.kind == ExpressionKind::Member && !structOf(*base.left)) {
			throw CompileError("assigning to " + quoted + " of a swizzle is not lowered yet");
		}

		const Expression* root = &base;
		while (root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) {
			root = root->left.get();
		}
		const auto rootBinding = _bindings.find(root->name);
		if (rootBinding != _bindings.end() && rootBinding->second.readOnly) {
			throw CompileError("assigning to " + quoted + " of \"" + root->name
				+ "\", which is const or in constant memory");
		}

		const Id address = emitPlaceAddress(base);
		const Id addressType = _builder.typeOf(address);
		const Id vectorType = _types.pointeeOf(addressType);
		const auto storageClass = _types.storageClassOf(addressType);
		if (!storageClass) {
			throw CompileError("assigning to " + quoted + " of a value that is not a local, "
				"a struct member or a buffer element");
		}
		const bool inBuffer = *storageClass == spirv::StorageClass::PhysicalStorageBuffer;
		if (_types.componentOf(vectorType) == InvalidId) {
			throw CompileError("assigning to " + quoted + " of a value that is not a vector");
		}
		requireLanesWithin(name, lanes, _types.vectorWidth(vectorType));

		Id value = emitExpression(valueExpression);
		const Id valueType = _builder.typeOf(value);
		const Id component = _types.componentOf(vectorType);
		const auto count = static_cast<uint32_t>(lanes.size());

		const Id old = inBuffer ? loadFromBuffer(address, vectorType) : loadFrom(address, vectorType);
		Id updated = InvalidId;
		if (count == 1) {
			if (_types.vectorWidth(valueType) != 1) {
				throw CompileError("assigning a vector of " + std::to_string(_types.vectorWidth(valueType))
					+ " components to " + quoted + ", which names one");
			}
			updated = _builder.emitTyped(spirv::OpCompositeInsert, vectorType,
				{ convert(value, valueType, component), old, lanes[0] });
		} else {
			const Id lanesType = _types.withWidth(vectorType, count);
			if (_types.vectorWidth(valueType) == 1) {
				value = broadcast(value, lanesType);
			} else if (valueType != lanesType) {
				throw CompileError("assigning to " + quoted + " takes a vector of " + std::to_string(count)
					+ " components of the same type as the target, or a scalar");
			}

			// Lane j keeps the old vector's component unless the swizzle names it,
			// in which case it takes the matching component of the value.
			std::vector<uint32_t> operands = { old, value };
			for (uint32_t lane = 0; lane < _types.vectorWidth(vectorType); ++lane) {
				uint32_t source = lane;
				for (uint32_t k = 0; k < count; ++k) {
					if (lanes[k] == lane) {
						source = _types.vectorWidth(vectorType) + k;
					}
				}
				operands.push_back(source);
			}
			updated = _builder.emitTyped(spirv::OpVectorShuffle, vectorType, operands);
		}

		if (inBuffer) {
			storeIntoBuffer(address, updated);
		} else {
			_builder.emit(spirv::OpStore, { address, updated });
		}
	}

	void Emitter::emitExpressionStatement(const Expression& expression) {
		// An assignment yields an address rather than a value, so it is
		// handled here rather than through emitExpression.
		if (expression.kind != ExpressionKind::Assign) {
			emitExpression(expression);
			return;
		}

		if (expression.left->kind == ExpressionKind::Member
			&& !structOf(*expression.left->left)) {
			emitSwizzleStore(*expression.left, *expression.right);
			return;
		}

		const Expression* root = expression.left.get();
		while (root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) {
			root = root->left.get();
		}
		if (root->kind == ExpressionKind::Identifier) {
			const auto target = _bindings.find(root->name);
			if (target != _bindings.end() && target->second.readOnly) {
				throw CompileError("cannot store through \"" + root->name + "\", which is in the "
					"constant address space or declared const");
			}
		}

		const Id address = emitPlaceAddress(*expression.left);

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
			case StatementKind::Compound: {
				const BindingScope scope(_bindings);
				// Whatever follows a return in the same block is unreachable,
				// and a block holds nothing after its terminator.
				for (const StatementPtr& child: statement.children) {
					if (_terminated) {
						break;
					}
					emitStatement(*child);
				}
				return;
			}

			case StatementKind::ExpressionStatement:
				if (statement.expression) {
					emitExpressionStatement(*statement.expression);
				}
				return;

			case StatementKind::DeclarationStatement:
				emitVariableDeclaration(*statement.declaration);
				return;

			case StatementKind::Return:
				emitReturn(statement);
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
				const BindingScope scope(_bindings);
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
		declareStageOutputs();

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
		loadStageInputs();
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
			_embeddedSamplers.clear();
			_nextBinding = 0;
			// The address block is per entry point too, and clearing it with the
			// rest is what keeps a later change to when it is read from inheriting
			// the previous function's. Nothing reads it while _bufferMembers is
			// empty today, so this is not a fix for an observable case.
			_addressBlock = {};
			_outputs.clear();
			_stageIn = nullptr;
			_stageInputs.clear();
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
			document += "\t\t\t],\n";
			document += "\t\t\t\"embedded_samplers\": [";
			for (size_t e = 0; e < _embeddedSamplers.size(); ++e) {
				const SamplerState& state = _embeddedSamplers[e].state;
				document += std::string(e ? "," : "") + "\n\t\t\t\t{ \"s_address\": \""
					+ samplerAddressName(state.sAddress)
					+ "\", \"t_address\": \"" + samplerAddressName(state.tAddress)
					+ "\", \"r_address\": \"" + samplerAddressName(state.rAddress)
					+ "\", \"mag_filter\": \"" + samplerFilterName(state.magFilter)
					+ "\", \"min_filter\": \"" + samplerFilterName(state.minFilter)
					+ "\", \"mip_filter\": \"" + samplerMipFilterName(state.mipFilter)
					+ "\", \"normalized_coordinates\": " + (state.normalizedCoordinates ? "true" : "false")
					+ ", \"compare_function\": \"Never\", \"anisotropy\": 1"
					+ ", \"border_color\": \"TransparentBlack\", \"lod_min\": 0, \"lod_max\": 65504"
					+ " }";
			}
			document += _embeddedSamplers.empty() ? "]\n" : "\n\t\t\t]\n";
			document += k + 1 < _entryPoints.size() ? "\t\t},\n" : "\t\t}\n";
		}

		document += "\t]\n}\n";

		// After every entry point, so a diagnostic about one function's own
		// interface comes before one about how two of them pair up.
		checkStageInterfacesAgree();

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
