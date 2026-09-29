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

	// OpFunction's FunctionControl mask. The generated table only covers value
	// enums, not bitmasks. Pure would promise no memory writes, which an entry
	// point that stores to a buffer breaks.
	constexpr uint32_t kFunctionControlNone = 0;

	// SelectionControl and LoopControl masks, likewise absent from the table.
	constexpr uint32_t kSelectionControlNone = 0;
	constexpr uint32_t kLoopControlNone = 0;

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

	// The stride is the element's own size, taken from the scalar width, since
	// an array of a vector has the vector's size.
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

bool TypeTable::isAggregate(spirv::Id type) const {
	if (_widthOfVector.count(type) > 0) {
		return true;
	}

	// _structs is keyed by name, so a struct is found by value.
	for (const auto& [name, id]: _structs) {
		if (id == type) {
			return true;
		}
	}

	return false;
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

Id TypeTable::namedStruct(const std::string& name) {
	const auto cached = _structs.find(name);
	if (cached != _structs.end()) {
		return cached->second;
	}

	const StructDecl* decl = _unit.findStruct(name);
	if (!decl) {
		return InvalidId;
	}

	std::vector<Id> fieldTypes;
	std::vector<uint32_t> offsets;
	uint32_t offset = 0;

	for (const StructField& field: decl->fields) {
		if (!field.type.isScalar() || !field.type.namedType.empty()) {
			throw CompileError("struct \"" + name + "\" has a field mslc cannot represent yet ("
				+ std::string(scalarKindName(field.type.scalar))
				+ (field.type.isPointer ? "*" : "")
				+ (field.type.isConst ? " const" : "")
				+ " " + field.name + ")");
		}

		const Id fieldType = scalar(field.type.scalar);
		const uint32_t size = mappingFor(field.type.scalar).width / 8;

		fieldTypes.push_back(fieldType);
		offsets.push_back(offset);
		offset += size;
	}


	// Members only. The count is implied by the instruction's word count, and
	// writing it as an operand would be read as one extra member.
	std::vector<uint32_t> operands;
	for (Id fieldType: fieldTypes) {
		operands.push_back(fieldType);
	}

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, operands);

	// A struct reached through a buffer binding is an interface block, so it
	// needs Block and per-member Offset. std140 rules apply, which for the
	// scalar members in the corpus is just their natural size and alignment.
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

const FunctionDecl* selectEntryPoint(const TranslationUnit& unit, Stage requested) {
	if (requested != Stage::None) {
		for (const FunctionDecl& function: unit.functions) {
			if (function.stage == requested) {
				return &function;
			}
		}

		throw CompileError(std::string("no ") + (requested == Stage::Kernel ? "kernel"
			: requested == Stage::Vertex ? "vertex" : "fragment")
			+ " entry point in this source");
	}

	const FunctionDecl* found = nullptr;
	for (const FunctionDecl& function: unit.functions) {
		if (function.stage == Stage::None) {
			continue;
		}

		if (found) {
			throw CompileError("source declares more than one entry point (\"" + found->name
				+ "\" and \"" + function.name + "\"); pass an explicit stage");
		}

		found = &function;
	}

	if (!found) {
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
	};

	class Emitter {
		spirv::Builder& _builder;
		const TranslationUnit& _unit;
		const FunctionDecl& _entryPoint;
		const ModuleOptions& _options;
		TypeTable& _types;

		std::map<std::string, Binding> _bindings;
		std::map<spirv::Id, const StructDecl*> _structByValue;

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
			const FunctionDecl& entryPoint, const ModuleOptions& options, TypeTable& types):
			_builder(builder), _unit(unit), _entryPoint(entryPoint),
			_options(options), _types(types) {}

		std::string run();

	private:
		Id constantU32(uint32_t value);
		Id declaredTypeOf(const Type& type);
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
		Id emitMemberAddress(const Expression& expression);
		Id emitCall(const Expression& expression);
		Id emitCast(const Expression& expression);
		Id emitIdentifier(const Expression& expression);
		Id loadFrom(Id pointer, Id pointeeType);
		Id convert(Id value, Id fromType, Id toType);
	};

	// No setSection here: OpConstant is routed to the types block by the
	// builder, and moving the current section would strand the caller's next
	// instruction in the wrong block.
	Id Emitter::constantU32(uint32_t value) {
		return _builder.emitDeclTyped(spirv::OpConstant, _uintType, { value });
	}

	Id Emitter::declaredTypeOf(const Type& type) {
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
		const uint32_t fromWidth = _types.bitWidth(fromType);
		const uint32_t toWidth = _types.bitWidth(toType);
		if (fromWidth == toWidth) {
			return _builder.emitTyped(spirv::OpBitcast, toType, { value });
		}

		const bool fromSigned = _types.isSignedInt(fromType);
		const bool toSigned = _types.isSignedInt(toType);

		// Truncation keeps the low bits whatever the signedness, so the result's
		// own signedness picks the opcode and the value survives.
		if (toWidth < fromWidth) {
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
		const auto temporary = integerKind(toWidth, fromSigned);
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
		const auto it = _bindings.find(expression.name);
		if (it == _bindings.end()) {
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

	Id Emitter::emitMember(const Expression& expression) {
		const Id address = emitMemberAddress(expression);

		// As with indexing, a member read yields the field and a member write
		// needs its address, so the address form does the work and this loads.
		auto it = _bindings.find(expression.left->name);
		if (it == _bindings.end() || !it->second.structType) {
			throw CompileError("\"" + expression.left->name + "\" has no struct type");
		}

		const StructDecl* decl = it->second.structType;
		for (const StructField& field: decl->fields) {
			if (field.name == expression.memberName) {
				return loadFrom(address, _types.scalar(field.type.scalar));
			}
		}

		throw CompileError("struct \"" + decl->name + "\" has no member \""
			+ expression.memberName + "\"");
	}

	Id Emitter::emitMemberAddress(const Expression& expression) {
		if (expression.left->kind != ExpressionKind::Identifier) {
			throw CompileError("member access on an expression is not supported yet");
		}

		const auto it = _bindings.find(expression.left->name);
		if (it == _bindings.end()) {
			throw CompileError("\"" + expression.left->name + "\" is not a parameter or local");
		}

		const StructDecl* decl = it->second.structType;
		if (!decl) {
			throw CompileError("\"" + expression.left->name + "\" is not a struct, so \"."
				+ expression.memberName + "\" is not a member of it");
		}

		size_t fieldIndex = decl->fields.size();
		for (size_t i = 0; i < decl->fields.size(); ++i) {
			if (decl->fields[i].name == expression.memberName) {
				fieldIndex = i;
				break;
			}
		}

		if (fieldIndex == decl->fields.size()) {
			throw CompileError("struct \"" + decl->name + "\" has no member \""
				+ expression.memberName + "\"");
		}

		// The result type is a pointer to the field, not to the struct the
		// parameter names. Indexing a struct yields the field, so a chain
		// typed as a pointer to the struct does not match what the base
		// indexes to, and spirv-val rejects it.
		const Id fieldType = _types.scalar(decl->fields[fieldIndex].type.scalar);

		return _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(it->second.storageClass, fieldType),
			{ it->second.id, constantU32(static_cast<uint32_t>(fieldIndex)) });
	}

	Id Emitter::emitCast(const Expression& expression) {
		const Id value = emitExpression(*expression.left);
		const Id fromType = _builder.typeOf(value);
		const Id toType = declaredTypeOf(*expression.castType);
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

		if (isLogical || isComparison) {
			const uint16_t opcode = isLogical
				? (expression.binaryOperator == BinaryOperator::LogicalAnd
					? spirv::OpLogicalAnd : spirv::OpLogicalOr)
				: comparisonOpcode(expression.binaryOperator, isFloat, isSigned);

			return _builder.emitTyped(opcode, _boolType, { left, right });
		}

		// OpUDiv and OpUMod need both operands of the result type, and a literal
		// is always uint. A shift count is free to have its own type and width.
		const bool isShift = expression.binaryOperator == BinaryOperator::ShiftLeft
			|| expression.binaryOperator == BinaryOperator::ShiftRight;
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
			case ExpressionKind::Assign: break;
		}

		// An assignment is a statement, so reaching it here means it was used
		// as a value, which MSL does not allow.
		throw CompileError("an assignment cannot be used as a value");
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

	void Emitter::declareParameters() {
		const std::vector<const Parameter*> implicitlyBound = assignImplicitBindings(_entryPoint);

		for (size_t index = 0; index < _entryPoint.parameters.size(); ++index) {
			const Parameter& parameter = _entryPoint.parameters[index];

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

				_builder.setSection(spirv::Section::Annotations);
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(spvBuiltin) });

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

			auto storageClass = storageClassForAddressSpace(parameter.type.addressSpace);
			if (!storageClass) {
				throw CompileError("parameter \"" + parameter.name + "\" needs a device, constant "
					"or threadgroup address space to be a buffer binding");
			}

			Id pointeeType = InvalidId;
			const StructDecl* structType = nullptr;

			if (!parameter.type.namedType.empty()) {
				pointeeType = _types.namedStruct(parameter.type.namedType);
				if (pointeeType == InvalidId) {
					throw CompileError("parameter \"" + parameter.name + "\" refers to undeclared "
						"type \"" + parameter.type.namedType + "\"");
				}
				structType = _unit.findStruct(parameter.type.namedType);
			} else {
				pointeeType = declaredTypeOf(parameter.type);
			}

			// A pointer parameter is a whole buffer, and Vulkan only accepts a
			// Block-decorated struct for a descriptor, so the element type is
			// wrapped in { T runtime_array[] } and the parameter indexes through
			// that.
			Id descriptorType = pointeeType;
			bool isBuffer = false;
			bool isReadOnly = false;
			if (parameter.type.isPointer && structType == nullptr) {
				descriptorType = _types.blockStructFor(pointeeType);
				isBuffer = true;

				// A runtime array breaks Uniform block layout, so a constant
				// pointer is a read-only storage buffer instead.
				if (*storageClass == spirv::StorageClass::Uniform) {
					storageClass = spirv::StorageClass::StorageBuffer;
					isReadOnly = true;
				}
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
			if (isReadOnly) {
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::NonWritable) });
			}

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

			_reflection += "\t\t{ \"kind\": \"Buffer\", \"metal_index\": "
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
			address = emitMemberAddress(*expression.left);
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

	std::string Emitter::run() {
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

		declareParameters();

		spirv::ExecutionModelValue model = spirv::ExecutionModel::GLCompute;
		if (_entryPoint.stage == Stage::Vertex) {
			model = spirv::ExecutionModel::Vertex;
		} else if (_entryPoint.stage == Stage::Fragment) {
			model = spirv::ExecutionModel::Fragment;
		}

		// The entry point's own id has to exist before it is named in
		// OpEntryPoint, so it is allocated here rather than after.
		_entryPointId = _builder.nextId();

		// A type declaration, so TypesGlobals rather than whatever section the
		// last decoration left behind. It also has to come after the types it
		// names, which it does because the parameters declared theirs already.
		_builder.setSection(spirv::Section::TypesGlobals);
		// No parameter count: like OpTypeStruct, the trailing variadic count is
		// derived from the instruction's word count and is not in the binary.
		// Writing one is read as a parameter type, which shows up as "Id is 0".
		const Id functionType = _builder.emitDecl(spirv::OpTypeFunction, { _voidType });

		_builder.setSection(spirv::Section::EntryPoints);
		{
			std::vector<uint32_t> operands = {
				static_cast<uint32_t>(model), _entryPointId
			};
			spirv::Builder::appendString(operands, _entryPoint.name);
			for (Id id: _interface) {
				operands.push_back(id);
			}
			_builder.emit(spirv::OpEntryPoint, operands);
		}

		if (_entryPoint.stage == Stage::Kernel) {
			_builder.setSection(spirv::Section::ExecutionModes);
			// 17 is LocalSize, read from the generated opcode table by name
			// rather than written here, since the enum is in spirv_opcodes.h.
			_builder.emit(spirv::OpExecutionMode, { _entryPointId, 17u,
				_options.localSizeX, _options.localSizeY, _options.localSizeZ });
		}

		_builder.setSection(spirv::Section::Functions);
		// The function's own id has to be the one the entry point names, so it
		// is written explicitly rather than taken from the emit's return.
		_builder.emitDeclTypedAt(spirv::OpFunction, _voidType, _entryPointId,
			{ kFunctionControlNone, functionType });
		beginBlock(_builder.nextId());
		emitFunctionBody(*_entryPoint.body);
		if (!_terminated) {
			terminate(spirv::OpReturn, { });
		}
		_builder.emit(spirv::OpFunctionEnd, { });

		return _reflection;
	}

}

std::string emitModule(spirv::Builder& builder, const TranslationUnit& unit,
	const FunctionDecl& entryPoint, const ModuleOptions& options) {
	TypeTable types(builder, unit);
	Emitter emitter(builder, unit, entryPoint, options, types);
	return emitter.run();
}

}
