#include "sema.h"

#include "lexer.h"
#include "parser.h"

#include <algorithm>
#include <cmath>
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

	// How many bytes a scalar takes in a buffer. A bool is one byte there, though
	// SPIR-V gives OpTypeBool no size of its own.
	uint32_t storageBytesOf(ScalarKind kind) {
		return kind == ScalarKind::Bool ? 1u : mappingFor(kind).width / 8;
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

	// The IEEE half nearest to a value, ties to even, as the bits OpConstant takes.
	// A finite value too large for a half becomes infinity.
	uint32_t halfBitsOf(double value) {
		if (std::isnan(value)) {
			return 0x7E00u;
		}

		const uint32_t sign = std::signbit(value) ? 0x8000u : 0u;
		const double magnitude = std::fabs(value);
		if (std::isinf(magnitude)) {
			return sign | 0x7C00u;
		}

		// Below the smallest normal the spacing is fixed at 2^-24, and a carry
		// into 0x400 is the smallest normal.
		if (magnitude < std::ldexp(1.0, -14)) {
			return sign | static_cast<uint32_t>(std::nearbyint(std::ldexp(magnitude, 24)));
		}

		int exponent = std::ilogb(magnitude);
		auto fraction = static_cast<uint32_t>(std::nearbyint(
			(std::ldexp(magnitude, -exponent) - 1.0) * 1024.0));
		if (fraction == 1024u) {
			fraction = 0u;
			++exponent;
		}

		if (exponent > 15) {
			return sign | 0x7C00u;
		}

		return sign | (static_cast<uint32_t>(exponent + 15) << 10) | fraction;
	}

	// The value a half's bits stand for, so a half literal can be folded at the
	// precision it really has.
	double halfValueOf(uint32_t bits) {
		const double sign = (bits & 0x8000u) ? -1.0 : 1.0;
		const int exponent = static_cast<int>((bits >> 10) & 0x1Fu);
		const uint32_t fraction = bits & 0x3FFu;
		if (exponent == 0x1F) {
			return fraction ? std::nan("") : sign * HUGE_VAL;
		}
		if (exponent == 0) {
			return sign * std::ldexp(static_cast<double>(fraction), -24);
		}

		return sign * std::ldexp(static_cast<double>(fraction | 0x400u), exponent - 25);
	}

	bool isIntegerKind(ScalarKind kind) {
		return kind >= ScalarKind::Char && kind <= ScalarKind::ULong;
	}

	// The 64 bits of a value as the kind holds them: reduced to its width, then
	// sign-extended for a signed kind and zero-extended for an unsigned one.
	uint64_t normalizeInteger(ScalarKind kind, uint64_t bits) {
		const uint32_t width = mappingFor(kind).width;
		if (width < 64) {
			bits &= (uint64_t{ 1 } << width) - 1;
			if (isSignedInteger(kind) && (bits >> (width - 1)) != 0) {
				bits |= ~uint64_t{ 0 } << width;
			}
		}

		return bits;
	}

	// The integer promotions: anything narrower than an int is an int.
	ScalarKind promotedKind(ScalarKind kind) {
		return mappingFor(kind).width < 32 ? ScalarKind::Int : kind;
	}

	// C's usual arithmetic conversions between two promoted integer kinds, which
	// is what usualArithmeticConversion does to the types of the same operands at
	// run time: the wider kind wins with its own signedness, and equal widths with
	// opposite signedness are both unsigned.
	ScalarKind commonIntegerKind(ScalarKind left, ScalarKind right) {
		const uint32_t leftWidth = mappingFor(left).width;
		const uint32_t rightWidth = mappingFor(right).width;
		if (leftWidth != rightWidth) {
			return leftWidth > rightWidth ? left : right;
		}

		if (left == right) {
			return left;
		}

		return leftWidth == 64 ? ScalarKind::ULong : ScalarKind::UInt;
	}

	// A float or half value rounded to what its own kind holds.
	double roundedToKind(ScalarKind kind, double value) {
		if (kind == ScalarKind::Half) {
			return halfValueOf(halfBitsOf(value));
		}

		return kind == ScalarKind::Float ? static_cast<double>(static_cast<float>(value)) : value;
	}

	// An integer constant as the float or half kind it is converted to. It goes
	// to a float from the integer itself rather than through a double, which
	// would round twice.
	double integerAsKind(ScalarKind kind, ScalarKind from, uint64_t integer) {
		if (kind == ScalarKind::Float) {
			return isSignedInteger(from)
				? static_cast<double>(static_cast<float>(static_cast<int64_t>(integer)))
				: static_cast<double>(static_cast<float>(integer));
		}

		return roundedToKind(kind, isSignedInteger(from)
			? static_cast<double>(static_cast<int64_t>(integer)) : static_cast<double>(integer));
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

	bool isStep(UnaryOperator op) {
		return op == UnaryOperator::PreIncrement || op == UnaryOperator::PreDecrement
			|| op == UnaryOperator::PostIncrement || op == UnaryOperator::PostDecrement;
	}

	const char* binaryOperatorSpelling(BinaryOperator op) {
		switch (op) {
			case BinaryOperator::Add: return "+";
			case BinaryOperator::Subtract: return "-";
			case BinaryOperator::Multiply: return "*";
			case BinaryOperator::Divide: return "/";
			case BinaryOperator::Modulo: return "%";
			case BinaryOperator::BitAnd: return "&";
			case BinaryOperator::BitOr: return "|";
			case BinaryOperator::BitXor: return "^";
			case BinaryOperator::ShiftLeft: return "<<";
			case BinaryOperator::ShiftRight: return ">>";
			default: break;
		}
		throw CompileError("this operator has no compound assignment form");
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
		Fwidth,
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
		{ "fwidth", 1, MathShape::Fwidth, kNoInstruction, kNoInstruction, kNoInstruction },
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
			if (child && !(expression.kind == ExpressionKind::Call && child == expression.left.get()
				&& child->kind == ExpressionKind::Identifier)) {
				collectIdentifiers(*child, names);
			}
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
		for (const SwitchCase& kase: statement.switchCases) {
			if (kase.value) collectIdentifiers(*kase.value, names);
			for (const StatementPtr& child: kase.body) {
				collectIdentifiers(*child, names);
			}
		}
		for (const StatementPtr& child: statement.switchPreamble) {
			collectIdentifiers(*child, names);
		}
	}

	// The identifiers a body names where no parameter or local of that name is in
	// scope, which is where a file-scope name is what they refer to. A local hides
	// the file-scope name from its declaration on, its own initialiser included.
	using ScopeStack = std::vector<std::set<std::string>>;

	void collectUnshadowed(const Expression* expression, const ScopeStack& scopes, std::set<std::string>& names) {
		if (!expression) return;
		std::set<std::string> mentioned;
		collectIdentifiers(*expression, mentioned);
		for (const std::string& name: mentioned) {
			const bool hidden = std::any_of(scopes.begin(), scopes.end(),
				[&](const std::set<std::string>& scope) { return scope.count(name) != 0; });
			if (!hidden) names.insert(name);
		}
	}

	void collectUnshadowed(const Statement* statement, ScopeStack& scopes, std::set<std::string>& names) {
		if (!statement) return;
		// A compound statement and a for loop open a scope; a declaration belongs to
		// the one around it.
		const bool opensScope = statement->kind == StatementKind::Compound || statement->kind == StatementKind::For
			|| statement->kind == StatementKind::Switch;
		if (opensScope) scopes.emplace_back();
		collectUnshadowed(statement->expression.get(), scopes, names);
		collectUnshadowed(statement->whileCondition.get(), scopes, names);
		for (const std::optional<VariableDeclaration>* declaration: { &statement->declaration, &statement->forInitializer }) {
			if (*declaration) {
				// The name is in scope in its own initialiser.
				scopes.back().insert((*declaration)->name);
				collectUnshadowed((*declaration)->initializer.get(), scopes, names);
			}
		}
		// The condition and increment of a for loop follow its initialiser.
		collectUnshadowed(statement->forCondition.get(), scopes, names);
		collectUnshadowed(statement->forIncrement.get(), scopes, names);
		for (const StatementPtr& child: statement->children) {
			collectUnshadowed(child.get(), scopes, names);
		}
		for (const Statement* nested: { statement->thenBranch.get(), statement->elseBranch.get(),
			statement->forBody.get(), statement->whileBody.get() }) {
			collectUnshadowed(nested, scopes, names);
		}
		for (const StatementPtr& child: statement->switchPreamble) {
			collectUnshadowed(child.get(), scopes, names);
		}
		for (const SwitchCase& kase: statement->switchCases) {
			collectUnshadowed(kase.value.get(), scopes, names);
			for (const StatementPtr& child: kase.body) {
				collectUnshadowed(child.get(), scopes, names);
			}
		}
		if (opensScope) scopes.pop_back();
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
		if (field.isPacked) {
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

uint32_t TypeTable::alignmentOf(Id type, bool packed) const {
	const uint32_t bits = bitWidth(type);
	if (bits == 0) {
		// A struct is as aligned as its most aligned member, which spirv-val needs
		// to be at least its largest scalar: 8 for a long.
		const std::string* name = structNameOf(type, nullptr);
		const StructDecl* decl = name ? _unit.findStruct(*name) : nullptr;
		if (!decl) {
			return 4;
		}

		uint32_t alignment = 1;
		for (const StructField& field: decl->fields) {
			const uint32_t components = field.type.vectorWidth > 1 ? field.type.vectorWidth : 1;
			const uint32_t scalarBytes = storageBytesOf(field.type.scalar);
			const VectorLayout layout = field.type.isMatrix()
				? matrixLayoutFor(scalarBytes, field.type.matrixColumns, components)
				: field.type.isPacked ? packedVectorLayoutFor(scalarBytes, components)
				: vectorLayoutFor(scalarBytes, components);
			alignment = std::max(alignment, layout.alignment);
		}

		return alignment;
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
		if (bits == 64) {
			throw CompileError("double is not supported");
		}

		words = { bits == 16 ? 0x3C00u : 0x3F800000u };
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

Id TypeTable::image(ResourceKind kind) {
	Id& cached = _images[kind];
	if (cached == InvalidId) {
		const uint32_t dimension = kind == ResourceKind::TextureCube ? spirv::Dim::Cube
			: kind == ResourceKind::Texture3D ? spirv::Dim::Dim3D : spirv::Dim::Dim2D;
		const uint32_t arrayed = kind == ResourceKind::Texture2DArray ? 1u : 0u;
		cached = _builder.emitDecl(spirv::OpTypeImage, { scalar(ScalarKind::Float),
			dimension, 2u, arrayed, 0u, 1u,
			static_cast<uint32_t>(spirv::ImageFormat::Unknown) });
	}

	return cached;
}

Id TypeTable::uintWriteImage() {
	if (_uintWriteImage == InvalidId) {
		_builder.emit(spirv::OpCapability, {
			static_cast<uint32_t>(spirv::Capability::StorageImageWriteWithoutFormat) });
		_uintWriteImage = _builder.emitDecl(spirv::OpTypeImage, { scalar(ScalarKind::UInt),
			spirv::Dim::Dim2D, 0u, 0u, 0u, 2u,
			static_cast<uint32_t>(spirv::ImageFormat::Unknown) });
	}
	return _uintWriteImage;
}

Id TypeTable::samplerType() {
	if (_samplerType == InvalidId) {
		_samplerType = _builder.emitDecl(spirv::OpTypeSampler);
	}

	return _samplerType;
}

Id TypeTable::sampledImage(ResourceKind kind) {
	const Id imageType = image(kind);
	Id& cached = _sampledImages[kind];
	if (cached == InvalidId) {
		cached = _builder.emitDecl(spirv::OpTypeSampledImage, { imageType });
	}

	return cached;
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
		const uint32_t scalarBytes = storageBytesOf(field.type.scalar);
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
		if (cached->second == InvalidId) {
			throw CompileError("recursive value struct \"" + name + "\" is not supported");
		}
		return cached->second;
	}
	const StructDecl* decl = _unit.findStruct(name);
	if (!decl) {
		return InvalidId;
	}
	_valueStructs.emplace(name, InvalidId);
	std::vector<Id> fieldTypes;
	for (const StructField& field: decl->fields) {
		if (field.type.isPointer || field.type.arrayLength) {
			throw CompileError(std::string(field.type.isPointer ? "a pointer" : "an array")
				+ " field in value struct \"" + name + "\" is not lowered yet");
		}
		const Id fieldType = !field.type.namedType.empty() ? valueStruct(field.type.namedType)
			: field.type.isMatrix() ? matrix(field.type.scalar, field.type.matrixColumns, field.type.vectorWidth)
			: field.type.vectorWidth > 1 ? vector(field.type.scalar, field.type.vectorWidth)
			: scalar(field.type.scalar);
		if (fieldType == InvalidId) {
			throw CompileError("undeclared type \"" + field.type.namedType + "\"");
		}
		fieldTypes.push_back(fieldType);
	}

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, fieldTypes);

	// Value structs need no buffer layout decorations.
	_valueStructs[name] = id;
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
		// Vulkan shader modules require an entry point; count helpers by name.
		std::set<std::string> names;
		for (const FunctionDecl& helper: unit.helpers) {
			names.insert(helper.name);
		}
		const size_t helpers = names.size();
		throw CompileError(helpers == 0
			? std::string("this source declares no function, and mslc emits one SPIR-V module per "
				"library: Vulkan requires an entry point, so a file with no kernel, vertex or "
				"fragment function cannot be compiled")
			: "this source declares " + std::to_string(helpers) + (helpers == 1 ? " helper function" : " helper functions")
				+ " and no kernel, vertex or fragment entry point. Apple's compiler builds a "
				"library of the helpers, but a Vulkan module needs an entry point, so mslc "
				"cannot compile a file whose functions are all helpers");
	}

	throw CompileError(std::string("no ") + (requested == Stage::Kernel ? "kernel"
		: requested == Stage::Vertex ? "vertex" : "fragment")
		+ " entry point in this source");
}

//
// Module emission
//

namespace {

	// SPIR-V constant operands cannot contain arithmetic or runtime loads.
	struct FoldedConstant {
		bool isComposite = false;
		Id type = InvalidId;
		std::vector<Id> parts;

		// Floats are rounded to their own width; integers are sign- or zero-extended
		// after truncation, so casting a signed value to int64_t preserves its value.
		ScalarKind scalar = ScalarKind::Void;
		double number = 0.0;
		uint64_t integer = 0;
		bool boolean = false;
	};

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
		// True when id is the address of an element or a member rather than a
		// variable, as for a reference local bound to one.
		bool isAddress = false;
		// True for a const local of integral type whose initialiser is a constant
		// expression: C++ lets it stand in a constant expression.
		bool constantValue = false;
		// True for a sampler declared with coord::pixel. Vulkan forbids an
		// implicit-lod lookup through an unnormalized sampler.
		bool unnormalizedSampler = false;
		std::optional<FoldedConstant> folded;
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

	// A scalar constant as the kind it is declared with. An integer wraps to the
	// width it is stored in, as a conversion does at run time, and goes to a
	// float or half with one rounding; a float may only become another float
	// kind, which the caller has checked.
	FoldedConstant convertConstant(FoldedConstant constant, ScalarKind to) {
		if (to == ScalarKind::Bool || constant.scalar == ScalarKind::Bool) {
			return constant;
		}

		if (isFloatKind(to)) {
			constant.number = isFloatKind(constant.scalar) ? roundedToKind(to, constant.number)
				: integerAsKind(to, constant.scalar, constant.integer);
		} else {
			constant.integer = normalizeInteger(to, constant.integer);
		}

		constant.scalar = to;
		return constant;
	}

	const char* cxxTypeName(ScalarKind kind) {
		switch (kind) {
			case ScalarKind::Char: return "char";
			case ScalarKind::UChar: return "unsigned char";
			case ScalarKind::Short: return "short";
			case ScalarKind::UShort: return "unsigned short";
			case ScalarKind::Int: return "int";
			case ScalarKind::UInt: return "unsigned int";
			case ScalarKind::Long: return "long";
			case ScalarKind::ULong: return "unsigned long";
			default: return scalarKindName(kind);
		}
	}

	std::string integerText(const FoldedConstant& constant) {
		return isSignedInteger(constant.scalar) ? std::to_string(static_cast<int64_t>(constant.integer))
			: std::to_string(constant.integer);
	}

	// A value inside a braced initialiser has to reach its element type without
	// changing, as in C++11, and Apple's compiler makes it an error where a
	// plain "constant int k = 2147483648;" is a warning. An integer has to fit
	// an integer type and be exactly representable in a float type, a float is
	// never narrowed to an integer, and a float only has to be in range for a
	// narrower float.
	void requireNotNarrowing(const FoldedConstant& constant, ScalarKind to) {
		const ScalarKind from = constant.scalar;
		const auto narrowed = [&]() {
			return CompileError("constant expression evaluates to " + (isFloatKind(from)
				? std::to_string(constant.number) : integerText(constant))
				+ " which cannot be narrowed to type '" + cxxTypeName(to) + "' in an initializer list");
		};

		if (isFloatKind(from)) {
			if (!isFloatKind(to)) {
				throw CompileError("type '" + std::string(cxxTypeName(from)) + "' cannot be narrowed to '"
					+ cxxTypeName(to) + "' in an initializer list");
			}

			if (std::isfinite(constant.number) && !std::isfinite(roundedToKind(to, constant.number))) {
				throw narrowed();
			}

			return;
		}

		const bool fromSigned = isSignedInteger(from);
		const bool negative = fromSigned && static_cast<int64_t>(constant.integer) < 0;
		const bool beyondLong = !fromSigned && static_cast<int64_t>(constant.integer) < 0;

		if (isFloatKind(to)) {
			// Apple, as clang does, converts the float back to the integer's own type
			// and compares. The conversion back saturates, so INT_MAX, which rounds
			// to 2^31 as a float and to infinity as a half, comes back as INT_MAX
			// and is let through.
			const double converted = integerAsKind(to, from, constant.integer);
			const __int128 value = fromSigned ? static_cast<__int128>(static_cast<int64_t>(constant.integer))
				: static_cast<__int128>(constant.integer);
			const __int128 width = static_cast<__int128>(1) << (mappingFor(from).width - (fromSigned ? 1 : 0));
			const __int128 low = fromSigned ? -width : 0;
			const __int128 high = width - 1;
			__int128 back = 0;
			if (converted >= static_cast<double>(high)) {
				back = high;
			} else if (converted <= static_cast<double>(low)) {
				back = low;
			} else {
				back = static_cast<__int128>(converted);
			}
			const bool exact = back == value;
			if (!exact) {
				throw narrowed();
			}

			return;
		}

		const bool toSigned = isSignedInteger(to);
		if ((negative && !toSigned) || (beyondLong && toSigned)
			|| normalizeInteger(to, constant.integer) != constant.integer) {
			throw narrowed();
		}
	}

	// Whether an integer case value is one the selector's type holds, which C++
	// asks of a case label where a braced initialiser asks it of a value.
	bool fitsInKind(const FoldedConstant& constant, ScalarKind to) {
		const uint32_t width = mappingFor(to).width;
		if (isSignedInteger(constant.scalar)) {
			const int64_t value = static_cast<int64_t>(constant.integer);
			if (isSignedInteger(to)) {
				if (width >= 64) {
					return true;
				}
				const int64_t low = -(int64_t{ 1 } << (width - 1));
				const int64_t high = (int64_t{ 1 } << (width - 1)) - 1;
				return value >= low && value <= high;
			}
			if (value < 0) {
				return false;
			}
			return width >= 64 || static_cast<uint64_t>(value) <= ((uint64_t{ 1 } << width) - 1);
		}

		const uint64_t value = constant.integer;
		if (isSignedInteger(to)) {
			return width >= 64 ? static_cast<int64_t>(value) >= 0
				: value <= ((uint64_t{ 1 } << (width - 1)) - 1);
		}
		return width >= 64 || value <= ((uint64_t{ 1 } << width) - 1);
	}

	// An Input or Output variable carrying one value across a stage boundary: a
	// returned value, or one field of a returned or [[stage_in]] struct. The
	// variable's type can differ from the value's, because a half crosses as a
	// float.
	struct StageVariable {
		Id variable = InvalidId;
		Id interfaceType = InvalidId;
		Id valueType = InvalidId;
	};


	// Every call a statement or an expression makes, by name, in source order.
	void collectCalls(const Expression& expression, std::vector<const Expression*>& out) {
		if (expression.kind == ExpressionKind::Call) {
			out.push_back(&expression);
		}

		for (const ExpressionPtr* child: { &expression.left, &expression.right }) {
			if (*child) {
				collectCalls(**child, out);
			}
		}
		for (const ExpressionPtr& argument: expression.arguments) {
			collectCalls(*argument, out);
		}
		for (const InitializerElement& element: expression.elements) {
			collectCalls(*element.value, out);
		}
	}

	void collectCalls(const VariableDeclaration& declaration, std::vector<const Expression*>& out) {
		if (declaration.initializer) {
			collectCalls(*declaration.initializer, out);
		}
	}

	void collectCalls(const Statement& statement, std::vector<const Expression*>& out) {
		for (const StatementPtr& child: statement.children) {
			collectCalls(*child, out);
		}
		for (const ExpressionPtr* expression: { &statement.expression, &statement.forCondition,
			&statement.forIncrement, &statement.whileCondition }) {
			if (*expression) {
				collectCalls(**expression, out);
			}
		}
		if (statement.declaration) {
			collectCalls(*statement.declaration, out);
		}
		if (statement.forInitializer) {
			collectCalls(*statement.forInitializer, out);
		}
		for (const StatementPtr* branch: { &statement.thenBranch, &statement.elseBranch,
			&statement.forBody, &statement.whileBody }) {
			if (*branch) {
				collectCalls(**branch, out);
			}
		}
		for (const SwitchCase& kase: statement.switchCases) {
			if (kase.value) {
				collectCalls(*kase.value, out);
			}
			for (const StatementPtr& child: kase.body) {
				collectCalls(*child, out);
			}
		}
		for (const StatementPtr& child: statement.switchPreamble) {
			collectCalls(*child, out);
		}
	}

	bool sameParameterTypes(const FunctionDecl& a, const FunctionDecl& b) {
		if (a.parameters.size() != b.parameters.size()) {
			return false;
		}

		for (size_t i = 0; i < a.parameters.size(); ++i) {
			const Type& x = a.parameters[i].type;
			const Type& y = b.parameters[i].type;
			if (a.parameters[i].isMutableReference() != b.parameters[i].isMutableReference()) {
				return false;
			}
			if (x.scalar != y.scalar || x.vectorWidth != y.vectorWidth || x.matrixColumns != y.matrixColumns
				|| x.isPacked != y.isPacked || x.namedType != y.namedType
				|| x.resource != y.resource || x.textureAccess != y.textureAccess) {
				return false;
			}
		}

		return true;
	}

	// Everything about the helper functions that the source alone decides, checked
	// for all of them whether or not an entry point reaches one: the declarations
	// of a name agree, a call names a function declared above it with the right
	// number of arguments, and no function reaches itself. Apple accepts a
	// recursive call, but SPIR-V for Vulkan forbids one, and a module with it
	// fails validation, so it is refused here with the cycle named.
	void validateHelpers(const TranslationUnit& unit) {
		std::map<std::string, const FunctionDecl*> first;
		std::map<std::string, const FunctionDecl*> defined;

		for (const FunctionDecl& helper: unit.helpers) {
			const auto declared = [&](const Type& type) {
				if (!type.namedType.empty() && !unit.findStruct(type.namedType)) {
					throw CompileError("undeclared type \"" + type.namedType + "\" in helper function \""
						+ helper.name + "\"");
				}
			};
			declared(helper.returnType);
			for (const Parameter& parameter: helper.parameters) {
				declared(parameter.type);
			}

			if (findMathBuiltin(helper.name)) {
				throw CompileError("helper function \"" + helper.name + "\" has the name of a builtin "
					"function; Apple's compiler reports a call that matches both as ambiguous, "
					"which mslc does not model");
			}
			if (unit.findFunction(helper.name)) {
				throw CompileError("\"" + helper.name + "\" names both an entry point and a helper function");
			}

			const auto previous = first.emplace(helper.name, &helper);
			if (!previous.second) {
				if (!sameParameterTypes(*previous.first->second, helper)) {
					throw CompileError("overloading \"" + helper.name + "\" is not lowered yet; Apple "
						"accepts a function declared again with other parameters, mslc takes one "
						"signature per name");
				}

				const Type& was = previous.first->second->returnType;
				const Type& now = helper.returnType;
				if (was.scalar != now.scalar || was.vectorWidth != now.vectorWidth
					|| was.matrixColumns != now.matrixColumns || was.namedType != now.namedType) {
					throw CompileError("\"" + helper.name + "\" is declared again with a different "
						"return type");
				}
			}

			if (helper.body && !defined.emplace(helper.name, &helper).second) {
				throw CompileError("redefinition of \"" + helper.name + "\"");
			}
		}

		std::map<std::string, std::set<std::string>> calls;
		const auto check = [&](const FunctionDecl& caller) {
			std::vector<const Expression*> found;
			collectCalls(*caller.body, found);

			for (const Expression* call: found) {
				if (call->left->kind != ExpressionKind::Identifier) {
					continue;
				}

				const std::string& name = call->left->name;
				if (unit.findFunction(name)) {
					throw CompileError("cannot call the entry point \"" + name + "\"");
				}

				const auto declared = first.find(name);
				if (declared == first.end()) {
					continue;
				}

				if (declared->second->order > caller.order) {
					throw CompileError("\"" + name + "\" is called before it is declared; declare it "
						"above its first use");
				}
				if (call->arguments.size() != declared->second->parameters.size()) {
					throw CompileError("call to \"" + name + "\" passes " + std::to_string(call->arguments.size())
						+ " arguments, and it takes " + std::to_string(declared->second->parameters.size()));
				}
				if (!defined.count(name)) {
					throw CompileError("\"" + name + "\" is declared but never defined");
				}

				if (caller.isHelper()) {
					calls[caller.name].insert(name);
				}
			}
		};

		for (const FunctionDecl& function: unit.functions) {
			check(function);
		}
		for (const auto& [name, definition]: defined) {
			check(*definition);
		}

		enum class Visit { Open, Done };
		std::map<std::string, Visit> state;
		std::vector<std::string> path;
		const auto visit = [&](const auto& self, const std::string& name) -> void {
			const auto known = state.find(name);
			if (known != state.end()) {
				if (known->second == Visit::Done) {
					return;
				}

				std::string cycle;
				const auto start = std::find(path.begin(), path.end(), name);
				for (auto it = start; it != path.end(); ++it) {
					cycle += *it + " -> ";
				}
				throw CompileError("recursive call: " + cycle + name + "; SPIR-V for Vulkan forbids "
					"recursion (Apple's compiler accepts it)");
			}

			state[name] = Visit::Open;
			path.push_back(name);
			for (const std::string& callee: calls[name]) {
				self(self, callee);
			}
			path.pop_back();
			state[name] = Visit::Done;
		};
		for (const auto& [name, definition]: defined) {
			visit(visit, name);
		}
	}

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
		// The file-scope samplers, by name, and the ones the entry point being
		// emitted names, which are looked up after its own parameters and locals.
		std::map<std::string, const VariableDeclaration*> _fileScopeSamplers;
		std::map<std::string, Binding> _fileScopeSamplerBindings;
		// The entry point whose sampler a helper that names a file-scope one holds.
		std::map<std::string, const FunctionDecl*> _samplerHelperOwner;
		// What each constant folded to, so a later constant referring to this one
		// needs the value rather than a reference to a constant.
		std::map<std::string, FoldedConstant> _folded;
		std::map<Id, FoldedConstant> _localFolded;
		// Set when a fold meets an operation a constant expression may not contain:
		// a signed overflow, a division by zero, a shift by a bad count, a NaN. The
		// value is still computed, since only a constexpr needs it to be an error.
		bool _foldDomainError = false;
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
		// Addresses of a bool or a bool vector in a buffer, each with the bool type
		// the access reads and writes. The address points at bytes, so what the
		// address itself says would be taken for a uchar.
		std::map<Id, Id> _boolAddresses;

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

		// The label of the block being emitted into, which an OpPhi names as the
		// predecessor a value arrives from.
		Id _currentBlock = InvalidId;

		// How many selection and loop constructs enclose the point being emitted,
		// counting the right operand of a scalar && or ||: spirv-val rejects a module
		// nested 1023 deep, whatever the source nesting that produced it.
		uint32_t _controlDepth = 0;

		// Where break and continue go from the point being emitted, innermost
		// last. A target with no continue label is one a switch could push: break
		// reaches it, continue looks past it to the loop around.
		struct JumpTarget {
			Id breakLabel;
			Id continueLabel;
		};
		std::vector<JumpTarget> _jumpTargets;

		std::string _reflection;
		// The next descriptor binding a texture or sampler takes, and the
		// constexpr samplers declared so far in the entry point.
		uint32_t _nextBinding = 0;
		std::vector<EmbeddedSampler> _embeddedSamplers;
		// Set once the module needs ImageQuery, which is declared once.
		bool _imageQueryDeclared = false;
		bool _sampleRateShadingDeclared = false;
		// Ids the entry point lists as its interface, in declaration order.
		std::vector<Id> _interface;

		// What a return statement writes: one variable for a returned scalar or
		// vector, or one per field of a returned struct. Empty for a void function.
		std::vector<StageVariable> _outputs;
		// The [[stage_in]] parameter and one Input per field.
		const Parameter* _stageIn = nullptr;
		std::vector<StageVariable> _stageInputs;

		// A helper's body is emitted once per stage that reaches it.
		struct HelperFunction {
			const FunctionDecl* definition = nullptr;
			std::map<Stage, Id> ids;
			Id returnType = InvalidId;
			std::vector<Id> parameterTypes;
			Id type = InvalidId;
		};
		std::map<std::string, HelperFunction> _helpers;
		// Helpers called and not yet emitted. A body cannot be emitted while the
		// function that calls it is half written, so they wait for its end.
		std::vector<HelperFunction*> _helperQueue;
		// A member call's receiver, evaluated early to learn its struct.
		Id _pendingObject = InvalidId;
		std::map<std::vector<Id>, Id> _functionTypes;
		// The helper whose body is being emitted, or null inside an entry point.
		const FunctionDecl* _helper = nullptr;

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
		// The address of a bool in a buffer, which holds bytes, recorded as one so
		// that a load and a store through it convert.
		Id boolAddress(Id storageAddress, Id boolType);
		// What an address points at, as the source sees it: the bool type for a
		// bool's bytes, the pointee otherwise.
		Id valueTypeAt(Id address) const;
		Id boolFromStorage(Id bytes, Id boolType);
		Id boolToStorage(Id value);
		// The Aligned memory operand a buffer access has to carry, at the
		// alignment the buffer's own layout guarantees.
		std::vector<uint32_t> alignedOperands(Id pointer, Id pointeeType) const;
		Id declaredTypeOf(const Type& type);
		void declareGlobals();
		Id declareGlobalConstant(const VariableDeclaration& declaration);
		Id emitConstant(const FoldedConstant& folded);
		std::vector<uint32_t> constantWords(const FoldedConstant& folded);
		FoldedConstant foldInitializer(const Type& type, const Expression& initializer, bool braced = false);
		void foldStructInitializer(FoldedConstant& folded, const StructDecl& decl,
			const Expression& initializer);
		FoldedConstant foldExpression(const Expression& expression);
		FoldedConstant foldBinary(const Expression& expression);
		FoldedConstant foldUnary(const Expression& expression);
		void declareParameters();
		void declareResources(const std::vector<std::pair<size_t, const Parameter*>>& resources);
		Id declareDescriptorVariable(Id pointee, uint32_t binding);
		void reserveLocalSamplers(const Statement& statement, const std::set<std::string>& used);
		void reserveSampler(const VariableDeclaration& declaration);
		// The entry point's own name first, then a file-scope sampler it names.
		const Binding* findResourceBinding(const std::string& name) const {
			const auto own = _bindings.find(name);
			if (own != _bindings.end()) return &own->second;
			const auto fileScope = _fileScopeSamplerBindings.find(name);
			return fileScope == _fileScopeSamplerBindings.end() ? nullptr : &fileScope->second;
		}
		Binding embeddedSamplerBinding(const VariableDeclaration& declaration) const;
		void reserveFileScopeSamplers();
		void rejectMemberFileScopeSamplers(const FunctionDecl& member) const;
		void declareLocalSampler(const VariableDeclaration& declaration);
		void addReflectionEntry(const std::string& entry);
		std::string descriptorJson(uint32_t binding) const;
		const Binding& textureReceiver(const Expression& call);
		void emitTextureWrite(const Expression& call, const Binding& texture);
		Id emitTextureCall(const Expression& call);
		Id emitTextureSample(const Expression& call, const Binding& texture);
		Id emitTextureRead(const Expression& call, const Binding& texture);
		Id emitTextureSize(const Expression& call, const Binding& texture, uint32_t component);
		Id appendArrayLayer(Id coordinate, ScalarKind component, const Expression& layer,
			const std::string& callName);
		Id texturePixels(const Binding& texture, Id sampled);
		Id emitLod(const Expression& expression, const std::string& callName);
		const StructDecl* structValue(const Type& type) const;
		StageVariable declareStageVariable(const Type& type, spirv::StorageClassValue storageClass,
			const std::string& what);
		void decorateInterpolation(Id variable, Interpolation interpolation);
		std::vector<StageVariable> declareFragmentOutputs(const StructDecl& decl);
		std::vector<StageVariable> declareStageStruct(const StructDecl& decl,
			spirv::StorageClassValue storageClass);
		std::vector<StageVariable> declareVertexAttributes(const StructDecl& decl);
		void declareStageOutputs();
		void loadStageInputs();
		void emitReturn(const Statement& statement);
		void checkStageInterfacesAgree() const;
		void emitFunctionBody(const Statement& statement);
		void emitStatement(const Statement& statement);
		void emitSwitch(const Statement& statement);
		void emitExpressionStatement(const Expression& expression);
		void emitAssignment(const Expression& left, const Expression& right,
			std::optional<BinaryOperator> compound);
		Id emitPlaceAddress(const Expression& left);
		const Expression& storeRoot(const Expression& target) const;
		void requireStorable(const Expression& target) const;
		void emitSwizzleStore(const Expression& target, const Expression& valueExpression,
			std::optional<BinaryOperator> compound);
		void emitVariableDeclaration(const VariableDeclaration& declaration, bool storageOnly = false);
		void bindLocal(const std::string& name, Id variable, Id type, const Type& msl);
		void beginBlock(Id label);
		void terminate(uint16_t opcode, std::vector<uint32_t> operands);
		void branchUnlessTerminated(Id label);

		// Every expression returns a value id whose type the builder knows.
		// For an lvalue such as "buffer[index]" the result is a pointer, and
		// the caller loads from it.
		Id emitExpression(const Expression& expression);
		Id emitBinary(const Expression& expression);
		Id emitBinaryOperation(BinaryOperator op, Id left, Id right);
		Id emitShortCircuit(BinaryOperator op, Id left, const Expression& rightExpression);
		Id emitConditional(const Expression& expression);
		bool isSafeToEvaluateUnchosen(const Expression& expression) const;
		Id conditionalType(Id trueType, Id falseType);
		Id toConditionalType(Id value, Id type);
		Id emitArithmetic(BinaryOperator op, Id left, Id right);
		Id emitVectorComparison(BinaryOperator op, Id left, Id right);
		Id emitCompoundOperation(BinaryOperator op, Id current, Id value);
		struct ArithmeticConversion {
			Id type;
			bool isSigned;
		};
		ArithmeticConversion usualArithmeticConversion(Id leftType, Id rightType);
		Id emitMatrixProduct(BinaryOperator op, Id left, Id right);
		Id emitUnary(const Expression& expression);
		void rejectBoolReference(const std::string& name, const Binding& binding) const;
		void emitReferenceDeclaration(const VariableDeclaration& declaration);
		void validateConstexprInitializer(const VariableDeclaration& declaration);
		bool readsRuntimeValue(const Expression& expression) const;
		void requireSameReferent(const VariableDeclaration& declaration, Id referent);
		Id emitIndex(const Expression& expression, bool asAddress);
		bool isElementAccess(const Expression& expression) const;
		Id emitElementAccess(const Expression& expression, bool asAddress);
		struct ElementIndex {
			std::optional<uint32_t> constant;
			Id id = InvalidId;
		};
		ElementIndex elementIndex(const Expression& expression, uint32_t width);
		Id wordIndex(Id index, const char* what);
		Id stepIndex(const Expression& expression, const char* what);
		Id emitMember(const Expression& expression);
		Id emitMemberAddress(const Expression& expression, Id& outFieldType);

		// The file-scope constant an expression names, or null when it names
		// something else. A constant is a value and not a place, which is what
		// tells a member read on one apart from a member read on a local.
		const ConstantBinding* constantFor(const Expression* expression);
		Id emitCall(const Expression& expression);
		HelperFunction* findHelper(const Expression& call, const Expression*& receiver);
		Id emitHelperCall(const Expression& call, HelperFunction& helper, const Expression* receiver);
		Id helperParameterType(const Type& type);
		void rejectAliasedReferenceArguments(const Expression& call, const FunctionDecl& definition,
			size_t first) const;
		Id referenceArgument(const Expression& argument, const Parameter& parameter,
			const std::string& function, size_t position, std::vector<std::pair<Id, Id>>& copyOuts);
		Id resourceArgument(const Expression& argument, const Parameter& parameter,
			const std::string& function);
		Id resourcePointee(const Type& type);
		void emitHelper(const HelperFunction& helper);
		void emitHelperReturn(const Statement& statement);
		bool returnsVoid(const FunctionDecl& function) const;
		Id emitMathBuiltin(const MathBuiltin& builtin, const std::vector<ExpressionPtr>& arguments);
		Id emitConstruct(const Expression& expression);
		Id emitConstructList(const Expression& expression, Id toType);
		Id emitInitListValue(const Type& target, const Expression& list);
		Id emitListScalarValue(const Type& target, const Expression& expression);
		Id convertListScalar(Id value, const Type& target, const Expression& expression);
		bool canFoldExpression(const Expression& expression, const std::string& excluded = {});
		const StructDecl* structOf(const Expression& expression);
		Id emitSwizzle(const Expression& expression);
		Id emitIdentifier(const Expression& expression);
		Id loadFrom(Id pointer, Id pointeeType);
		Id promotedTo(uint32_t components) const;
		Id convert(Id value, Id fromType, Id toType);
		Id convertStruct(Id value, const TypeTable::StructForm& from, const TypeTable::StructForm& to, Id toType);
		Id convertImplicit(Id value, Id toType);
		Id asCondition(Id value);
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
		const auto cached = _bufferBases.find(binding.memberIndex);
		if (cached == _bufferBases.end()) {
			throw CompileError("no address was loaded for this buffer, so it cannot be used");
		}

		return cached->second;
	}

	// A struct is declared once as a value and once as a buffer's laid-out
	// element, so a copy between a local and a buffer is two types for one struct.
	// OpBitcast takes no struct, so the copy is member by member: the members are
	// pulled out of the source and the target is built from them, with a packed
	// vector turned between the array of components a laid-out struct stores and
	// the vector a local holds.
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

			if (from.declared[i] != to.declared[i] && _types.isBool(from.value[i])) {
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

		return _builder.emitTyped(spirv::OpCompositeConstruct, toType, members);
	}

	Id Emitter::convert(Id value, Id fromType, Id toType) {
		if (fromType == toType) {
			return value;
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
			_types.scalar(*temporary), { value });
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

	// A bool is stored as a byte, which an indexed buffer maps through boolAddress;
	// the base of a reference parameter has no such mapping yet.
	void Emitter::rejectBoolReference(const std::string& name, const Binding& binding) const {
		if (_types.isBool(binding.pointeeType)) {
			throw CompileError("\"" + name + "\" is a bool reference parameter, which is not lowered "
				"yet; pass a pointer to it, or put it in a struct");
		}
	}

	Id Emitter::emitIdentifier(const Expression& expression) {
		// The entry point's own names first, then the module's: a constant declared
		// at file scope is not a parameter of the entry point that reads it.
		const Binding* found = findResourceBinding(expression.name);
		if (!found) {
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

		if (binding.pointeeMsl.resource != ResourceKind::None) {
			throw CompileError("\"" + expression.name + "\" is a " + typeName(binding.pointeeMsl)
				+ ", which mslc uses as the receiver of a texture call or as sample's sampler "
					"argument only");
		}

		// A buffer is reached through the address block, so the binding has no id
		// of its own to load, and loading it wrote an OpLoad of id 0.
		if (binding.bufferPointeeType != InvalidId) {
			if (binding.isBuffer) {
				throw CompileError("the buffer \"" + expression.name + "\" is used as a value, "
					"which is not lowered yet");
			}

			// A reference parameter names the one value its buffer holds, so its
			// use as a value reads the whole of it.
			rejectBoolReference(expression.name, binding);
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
		if (isElementAccess(expression)) {
			return emitElementAccess(expression, asAddress);
		}

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

		const Id index = stepIndex(*expression.arguments[0],
			binding.bufferPointeeType != InvalidId ? "buffer element" : "matrix column");

		// A buffer is reached through its address rather than a descriptor, so
		// the access chain starts from the loaded pointer. The result is a
		// pointer in the buffer's own storage class, because the result class of
		// an access chain has to match its base.
		if (binding.bufferPointeeType != InvalidId) {
			const Id base = bufferBase(binding);
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

		// Indexing a matrix names one of its columns, not another matrix.
		const auto* matrix = _types.matrixInfo(binding.pointeeType);
		const Id elementType = matrix ? matrix->column : binding.pointeeType;
		const Id address = _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(binding.storageClass, elementType),
			{ binding.id, index });

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

		const auto it = _bindings.find(expression.left->name);
		return it != _bindings.end() && !it->second.isBuffer && !_types.matrixInfo(it->second.pointeeType);
	}

	// The index of a literal, with a sign. Anything else is not known here and is
	// emitted and indexed at run time.
	static std::optional<int64_t> literalIndex(const Expression& expression) {
		constexpr uint64_t kBeyondAnyWidth = uint64_t(1) << 40;
		switch (expression.kind) {
			case ExpressionKind::BoolLiteral: return expression.boolValue ? 1 : 0;
			case ExpressionKind::IntLiteral:
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
		const Id index = wordIndex(emitExpression(expression), what);
		_builder.setType(index, _uintType);
		return index;
	}

	// v[i] where v is a vector: one component of it. As a value it is taken out of
	// the loaded vector. As a place it is an access chain into the vector's own
	// storage, so a store changes that lane and no other.
	Id Emitter::emitElementAccess(const Expression& expression, bool asAddress) {
		const Expression& base = *expression.left;
		const std::string notAVector = "only an element of a vector is lowered here; a matrix element "
			"of a buffer or a struct member is not lowered yet";

		if (!asAddress) {
			const Id vector = emitExpression(base);
			const Id type = _builder.typeOf(vector);
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

	// Distinguish struct members from vector swizzles before emitting the left side.
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
		return it != _bindings.end() && (it->second.bufferPointeeType != InvalidId
				|| it->second.storageClass == spirv::StorageClass::PhysicalStorageBuffer)
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

				operands.push_back(stepIndex(*step.index, "buffer element"));
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
		// A reference local bound to a buffer element holds that element's address
		// and has no buffer of its own, but its fields are laid out the same way.
		const bool inBuffer = fromBuffer || binding.storageClass == spirv::StorageClass::PhysicalStorageBuffer;
		const bool packed = inBuffer && current.isPacked;
		const bool bytes = inBuffer && _types.isBool(outFieldType);
		const Id address = _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(storageClass, packed
				? _types.packedStorage(current.scalar, current.vectorWidth)
				: bytes ? _types.boolStorage(outFieldType) : outFieldType), operands);

		return packed ? packedVectorAddress(address, outFieldType)
			: bytes ? boolAddress(address, outFieldType) : address;
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

	bool Emitter::canFoldExpression(const Expression& expression, const std::string& excluded) {
		switch (expression.kind) {
			case ExpressionKind::IntLiteral:
			case ExpressionKind::BoolLiteral: return true;
			case ExpressionKind::FloatLiteral: return std::isfinite(foldExpression(expression).number);
			case ExpressionKind::Identifier: {
				if (expression.name == excluded) { return false; }
				const auto local = _bindings.find(expression.name);
				if (local != _bindings.end()) { return _localFolded.count(local->second.id) != 0; }
				const auto value = _folded.find(expression.name);
				if (value == _folded.end() || value->second.isComposite
					|| (isFloatKind(value->second.scalar) && !std::isfinite(value->second.number))) { return false; }
				for (const VariableDeclaration& global: _unit.globals) {
					if (global.name != expression.name || !global.initializer) { continue; }
					const Expression* initializer = global.initializer.get();
					while (initializer->kind == ExpressionKind::InitList && initializer->elements.size() == 1) {
						initializer = initializer->elements.front().value.get();
					}
					const Expression* literal = initializer;
					while (literal->kind == ExpressionKind::Unary) { literal = literal->left.get(); }
					return (literal->kind == ExpressionKind::IntLiteral || literal->kind == ExpressionKind::FloatLiteral
						|| literal->kind == ExpressionKind::BoolLiteral) && canFoldExpression(*initializer);
				}
				return false;
			}
			case ExpressionKind::Unary: {
				const UnaryOperator op = expression.unaryOperator;
				if ((op != UnaryOperator::Plus && op != UnaryOperator::Negate
					&& op != UnaryOperator::BitNot && op != UnaryOperator::Not)
					|| !canFoldExpression(*expression.left, excluded)) { return false; }
				const FoldedConstant operand = foldExpression(*expression.left);
				if ((operand.scalar == ScalarKind::Bool && op != UnaryOperator::Not)
					|| (isFloatKind(operand.scalar) && op == UnaryOperator::BitNot)) { return false; }
				if (op == UnaryOperator::Negate && isSignedInteger(operand.scalar)) {
					const ScalarKind promoted = promotedKind(operand.scalar);
					const uint64_t minimum = normalizeInteger(promoted, uint64_t{ 1 } << (mappingFor(promoted).width - 1));
					if (operand.integer == minimum) { return false; }
				}
				return true;
			}
			default: return false;
		}
	}

	Id Emitter::convertListScalar(Id value, const Type& target, const Expression& expression) {
		const auto from = _types.scalarKindOf(_builder.typeOf(value));
		if (!from) { throw CompileError("a scalar initializer list needs a scalar value"); }
		const ScalarKind to = target.scalar;
		const bool integerWidening = !isFloatKind(*from) && !isFloatKind(to)
			&& (isSignedInteger(*from) == isSignedInteger(to)
				? mappingFor(*from).width <= mappingFor(to).width
				: !isSignedInteger(*from) && mappingFor(*from).width < mappingFor(to).width);
		if (*from != to && !(*from == ScalarKind::Half && to == ScalarKind::Float) && !integerWidening) {
			if (isFloatKind(*from) && !isFloatKind(to)) {
				throw CompileError("a floating value cannot be narrowed to an integer or bool in an initializer list");
			}
			if (!canFoldExpression(expression)) {
				throw CompileError("a narrowing initializer list conversion requires a supported constant expression; this form is not lowered yet");
			}
			FoldedConstant folded = foldExpression(expression);
			if (folded.scalar == ScalarKind::Bool) { folded.integer = folded.boolean ? 1 : 0; }
			requireNotNarrowing(folded, to);
		}
		return convertImplicit(value, _types.scalar(to));
	}

	Id Emitter::emitListScalarValue(const Type& target, const Expression& expression) {
		if (expression.kind != ExpressionKind::InitList) {
			return convertListScalar(emitExpression(expression), target, expression);
		}
		if (expression.elements.empty()) { return _types.zero(declaredTypeOf(target)); }
		if (expression.elements.size() != 1) {
			throw CompileError("nested scalar initialization with excess elements is not lowered yet");
		}
		if (!expression.elements.front().fieldName.empty()) {
			throw CompileError("a field designator requires a struct initializer");
		}
		return emitListScalarValue(target, *expression.elements.front().value);
	}

	Id Emitter::emitInitListValue(const Type& target, const Expression& list) {
		if (target.addressSpace != AddressSpace::None) {
			throw CompileError("a local in the " + std::string(addressSpaceName(target.addressSpace))
				+ " address space is not lowered yet; every local mslc declares is thread-private");
		}
		const Id toType = declaredTypeOf(target);
		const std::string spelled = typeName(target);
		if (!target.namedType.empty()) {
			const StructDecl& decl = *_unit.findStruct(target.namedType);
			if (decl.hasConstructors) {
				throw CompileError("brace initialization of " + spelled + " with declared constructors is not lowered yet");
			}
			std::optional<Id> singleValue;
			if (list.elements.size() == 1 && list.elements.front().fieldName.empty()
				&& list.elements.front().value->kind != ExpressionKind::InitList) {
				singleValue = emitExpression(*list.elements.front().value);
				const std::string* sourceName = _types.structNameOf(_builder.typeOf(*singleValue), nullptr);
				if (sourceName && *sourceName == target.namedType) {
					return convertImplicit(*singleValue, toType);
				}
			}
			Expression empty;
			empty.kind = ExpressionKind::InitList;
			std::vector<uint32_t> fields;
			for (const InitializerElement& element: list.elements) {
				if (!element.fieldName.empty()) {
					size_t named = fields.size();
					while (named < decl.fields.size() && decl.fields[named].name != element.fieldName) {
						++named;
					}
					if (named == decl.fields.size()) {
						throw CompileError("field " + element.fieldName + " is unknown or out of order in " + spelled + " initializer");
					}
					while (fields.size() < named) {
						fields.push_back(emitInitListValue(decl.fields[fields.size()].type, empty));
					}
				}
				if (fields.size() == decl.fields.size()) {
					throw CompileError("excess elements in " + spelled + " initializer");
				}
				const Type& field = decl.fields[fields.size()].type;
				if (element.value->kind == ExpressionKind::InitList) {
					fields.push_back(emitInitListValue(field, *element.value));
				} else {
					const Id value = singleValue ? *singleValue : emitExpression(*element.value);
					fields.push_back(field.namedType.empty() && field.isScalar()
						? convertListScalar(value, field, *element.value)
						: convertImplicit(value, declaredTypeOf(field)));
				}
			}
			while (fields.size() < decl.fields.size()) {
				fields.push_back(emitInitListValue(decl.fields[fields.size()].type, empty));
			}
			return _builder.emitTyped(spirv::OpCompositeConstruct, toType, fields);
		}
		if (list.elements.empty()) { return _types.zero(toType); }
		for (const InitializerElement& element: list.elements) {
			if (!element.fieldName.empty()) { throw CompileError("a field designator requires a struct initializer"); }
		}
		if (target.isMatrix()) {
			if (list.elements.size() != target.matrixColumns) {
				throw CompileError("a matrix initializer needs one vector per column");
			}
			Type columnType;
			columnType.scalar = target.scalar;
			columnType.vectorWidth = target.vectorWidth;
			std::vector<uint32_t> columns;
			for (const InitializerElement& element: list.elements) {
				if (element.value->kind == ExpressionKind::InitList && element.value->elements.empty()) {
					throw CompileError("an empty matrix column initializer is not lowered yet");
				}
				const Id column = element.value->kind == ExpressionKind::InitList
					? emitInitListValue(columnType, *element.value) : emitExpression(*element.value);
				if (_builder.typeOf(column) != declaredTypeOf(columnType)) {
					throw CompileError("a matrix initializer column has to have its column type already");
				}
				columns.push_back(column);
			}
			return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
		}
		Type component;
		component.scalar = target.scalar;
		if (target.isScalar()) {
			if (list.elements.size() != 1) { throw CompileError("excess elements in a scalar initializer list"); }
			return emitListScalarValue(component, *list.elements.front().value);
		}
		std::vector<uint32_t> pieces;
		uint32_t width = 0;
		for (const InitializerElement& element: list.elements) {
			Id value = element.value->kind == ExpressionKind::InitList
				? emitListScalarValue(component, *element.value) : emitExpression(*element.value);
			const Id valueType = _builder.typeOf(value);
			if (_types.scalarKindOf(valueType)) {
				if (element.value->kind != ExpressionKind::InitList) { value = convertListScalar(value, component, *element.value); }
				++width;
			} else {
				if (list.elements.size() == 1 && valueType == toType) { return value; }
				throw CompileError("a vector initializer list needs scalar elements or one value of its exact vector type");
			}
			pieces.push_back(value);
		}
		if (width > target.vectorWidth) { throw CompileError("a vector initializer list has too many components"); }
		while (pieces.size() < target.vectorWidth) { pieces.push_back(_types.zero(_types.scalar(component.scalar))); }
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

		TypeTable::StructForm form;
		if (_types.structForm(type, form)) {
			throw CompileError("a unary operator on the struct " + form.name + " is not lowered");
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
				// A bool vector is negated componentwise, and stays one.
				return _types.isBool(type)
					? _builder.emitTyped(spirv::OpLogicalNot, type, { operand })
					: _builder.emitTyped(spirv::OpLogicalNot, _boolType, { asCondition(operand) });
			case UnaryOperator::BitNot:
				return _builder.emitTyped(spirv::OpNot, type, { operand });
			default:
				throw CompileError("++ and -- are lowered only as a statement or a for-loop "
					"increment, not for their value");
		}
	}

	Emitter::ArithmeticConversion Emitter::usualArithmeticConversion(Id leftOperandType, Id rightOperandType) {
		// The two operands have to share a type, and the rule is C's usual
		// arithmetic conversions: a float beats an integer, then the wider
		// integer beats the narrower, and only then does signedness break a tie
		// with both becoming unsigned. Converting one side to the other's type
		// instead narrows: "3 < f" would compare as ints and "b < v" for a uchar
		// and an int would compare as uchars.
		Id leftType = leftOperandType;
		Id rightType = rightOperandType;

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
			// unsigned comparison of 4294967295 against the value. It is taken from
			// the width, so a 64-bit operand stays 64 bits.
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

		return { commonType, operandsSigned };
	}

	// Holds one level of structured control flow for as long as it lives.
	class ControlDepthScope {
	public:
		explicit ControlDepthScope(uint32_t& depth): _depth(depth) {
			constexpr uint32_t kMaxControlDepth = 1000;
			if (_depth >= kMaxControlDepth) {
				throw CompileError("control flow is nested more than "
					+ std::to_string(kMaxControlDepth) + " deep");
			}
			++_depth;
		}
		~ControlDepthScope() { --_depth; }
		ControlDepthScope(const ControlDepthScope&) = delete;
		ControlDepthScope& operator=(const ControlDepthScope&) = delete;

	private:
		uint32_t& _depth;
	};

	// A scalar && or || evaluates its right operand only when the left does not
	// decide the result, so a guard such as `i < n && buf[i] > 0` is one. A vector
	// operand is componentwise and evaluates both, as in Metal.
	Id Emitter::emitShortCircuit(BinaryOperator op, Id left, const Expression& rightExpression) {
		const bool isAnd = op == BinaryOperator::LogicalAnd;
		const Id condition = asCondition(left);
		const Id leftBlock = _currentBlock;
		const Id rightLabel = _builder.nextId();
		const Id mergeLabel = _builder.nextId();

		_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
		terminate(spirv::OpBranchConditional, { condition,
			isAnd ? rightLabel : mergeLabel, isAnd ? mergeLabel : rightLabel });

		beginBlock(rightLabel);
		const ControlDepthScope depth(_controlDepth);
		const Id right = asCondition(emitExpression(rightExpression));
		const Id rightBlock = _currentBlock;
		terminate(spirv::OpBranch, { mergeLabel });

		beginBlock(mergeLabel);
		const Id decided = isAnd
			? _builder.emitDeclTyped(spirv::OpConstantFalse, _boolType, { })
			: _builder.emitDeclTyped(spirv::OpConstantTrue, _boolType, { });
		return _builder.emitTyped(spirv::OpPhi, _boolType, { decided, leftBlock, right, rightBlock });
	}

	// Whether evaluating an expression whether or not it is chosen is harmless: it
	// reads no memory through an index, a pointer, a reference or a call, divides
	// nothing and branches nowhere, so it cannot trap or touch an address the
	// condition guards. A name bound to a buffer is such a pointer or reference,
	// and so is every member reached through one.
	bool Emitter::isSafeToEvaluateUnchosen(const Expression& expression) const {
		const auto safe = [this](const ExpressionPtr& child) {
			return !child || isSafeToEvaluateUnchosen(*child);
		};

		switch (expression.kind) {
			case ExpressionKind::IntLiteral:
			case ExpressionKind::FloatLiteral:
			case ExpressionKind::BoolLiteral:
				return true;
			case ExpressionKind::Identifier: {
				const Binding* binding = findResourceBinding(expression.name);
				return !binding || (binding->bufferPointeeType == InvalidId
					&& binding->storageClass != spirv::StorageClass::PhysicalStorageBuffer);
			}
			case ExpressionKind::Member:
				return safe(expression.left);
			case ExpressionKind::Unary:
				// ++ and -- write, and are not values to compute and discard.
				switch (expression.unaryOperator) {
					case UnaryOperator::Negate:
					case UnaryOperator::Plus:
					case UnaryOperator::Not:
					case UnaryOperator::BitNot:
						return safe(expression.left);
					default:
						return false;
				}
			case ExpressionKind::Binary:
				return expression.binaryOperator != BinaryOperator::Divide
					&& expression.binaryOperator != BinaryOperator::Modulo
					&& expression.binaryOperator != BinaryOperator::LogicalAnd
					&& expression.binaryOperator != BinaryOperator::LogicalOr
					&& safe(expression.left) && safe(expression.right);
			case ExpressionKind::Construct:
			case ExpressionKind::Conditional: {
				if (!safe(expression.left)) {
					return false;
				}
				for (const ExpressionPtr& argument: expression.arguments) {
					if (!safe(argument)) {
						return false;
					}
				}
				return true;
			}
			case ExpressionKind::Index:
			case ExpressionKind::Call:
			case ExpressionKind::InitList:
			case ExpressionKind::Assign:
				return false;
		}

		return false;
	}

	// The type both values of a conditional have: the same type stays as it is,
	// as in C++ where two shorts give a short, and two different scalars meet by
	// the usual arithmetic conversions. A scalar beside a vector joins the
	// vector's component type, and two vectors have to be one type.
	Id Emitter::conditionalType(Id trueType, Id falseType) {
		if (trueType == falseType) {
			return trueType;
		}

		TypeTable::StructForm form;
		if (_types.structForm(trueType, form) || _types.structForm(falseType, form)) {
			// Forms of one struct convert to each other; convert() refuses anything else.
			return trueType;
		}

		if (_types.matrixInfo(trueType) || _types.matrixInfo(falseType)) {
			throw CompileError("the two values of a conditional are matrices of different types");
		}

		const uint32_t trueWidth = _types.vectorWidth(trueType);
		const uint32_t falseWidth = _types.vectorWidth(falseType);

		if (trueWidth > 1 && falseWidth > 1) {
			throw CompileError(trueWidth != falseWidth
				? "the two values of a conditional are vectors of different widths"
				: "the two values of a conditional are vectors of different types");
		}

		if (trueWidth > 1 || falseWidth > 1) {
			const Id vector = trueWidth > 1 ? trueType : falseType;
			const Id scalar = trueWidth > 1 ? falseType : trueType;
			if (_types.isFloat(scalar) && !_types.isFloat(vector)) {
				throw CompileError("a floating-point scalar cannot be combined with an integer vector");
			}
			return vector;
		}

		return usualArithmeticConversion(trueType, falseType).type;
	}

	Id Emitter::toConditionalType(Id value, Id type) {
		const Id from = _builder.typeOf(value);
		if (_types.vectorWidth(type) > 1 && _types.vectorWidth(from) == 1) {
			return broadcast(value, type);
		}

		return convert(value, from, type);
	}

	// `c ? a : b` evaluates one of its values. When neither can trap or touch
	// memory, evaluating both and selecting is the same and costs no branch.
	// Otherwise the values go in their own blocks and meet in an OpPhi, as in
	// emitShortCircuit, so `i < n ? buf[i] : 0.0f` never reads buf[n].
	Id Emitter::emitConditional(const Expression& expression) {
		const Id condition = asCondition(emitExpression(*expression.left));
		const Expression& whenTrue = *expression.arguments[0];
		const Expression& whenFalse = *expression.arguments[1];

		if (isSafeToEvaluateUnchosen(whenTrue) && isSafeToEvaluateUnchosen(whenFalse)) {
			Id trueValue = emitExpression(whenTrue);
			Id falseValue = emitExpression(whenFalse);
			const Id type = conditionalType(_builder.typeOf(trueValue), _builder.typeOf(falseValue));
			trueValue = toConditionalType(trueValue, type);
			falseValue = toConditionalType(falseValue, type);
			return _builder.emitTyped(spirv::OpSelect, type, { condition, trueValue, falseValue });
		}

		const Id trueLabel = _builder.nextId();
		const Id falseLabel = _builder.nextId();
		const Id mergeLabel = _builder.nextId();

		_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
		terminate(spirv::OpBranchConditional, { condition, trueLabel, falseLabel });

		// The common type is known only once the second value is emitted, and the
		// first block must convert to it before it ends: emit the first, lift it
		// out unfinished, emit the second, then put the first back ahead of it.
		const size_t mark = _builder.functionsMark();
		beginBlock(trueLabel);
		Id trueValue = emitExpression(whenTrue);
		const Id trueEnd = _currentBlock;
		std::vector<spirv::Instruction> trueBody = _builder.takeFunctionsFrom(mark);

		beginBlock(falseLabel);
		Id falseValue = emitExpression(whenFalse);
		const Id falseEnd = _currentBlock;
		const Id type = conditionalType(_builder.typeOf(trueValue), _builder.typeOf(falseValue));
		falseValue = toConditionalType(falseValue, type);
		terminate(spirv::OpBranch, { mergeLabel });
		std::vector<spirv::Instruction> falseBody = _builder.takeFunctionsFrom(mark);

		_builder.appendFunctions(std::move(trueBody));
		trueValue = toConditionalType(trueValue, type);
		terminate(spirv::OpBranch, { mergeLabel });
		_builder.appendFunctions(std::move(falseBody));

		beginBlock(mergeLabel);
		return _builder.emitTyped(spirv::OpPhi, type, { trueValue, trueEnd, falseValue, falseEnd });
	}

	// Whether an expression may split into several blocks: a && or || on scalars,
	// or a conditional whose values branch. Counted whether or not it does.
	static bool containsBranchingOperator(const Expression& expression) {
		if (expression.kind == ExpressionKind::Conditional
			|| (expression.kind == ExpressionKind::Binary
				&& (expression.binaryOperator == BinaryOperator::LogicalAnd
					|| expression.binaryOperator == BinaryOperator::LogicalOr))) {
			return true;
		}

		const auto contains = [](const ExpressionPtr& child) {
			return child && containsBranchingOperator(*child);
		};
		if (contains(expression.left) || contains(expression.right)) {
			return true;
		}
		for (const ExpressionPtr& argument: expression.arguments) {
			if (contains(argument)) {
				return true;
			}
		}
		return false;
	}

	Id Emitter::emitBinary(const Expression& expression) {
		const Id left = emitExpression(*expression.left);

		const bool isLogical = expression.binaryOperator == BinaryOperator::LogicalAnd
			|| expression.binaryOperator == BinaryOperator::LogicalOr;
		const Id leftType = _builder.typeOf(left);
		if (isLogical && !_types.matrixInfo(leftType) && _types.vectorWidth(leftType) <= 1) {
			return emitShortCircuit(expression.binaryOperator, left, *expression.right);
		}

		const Id right = emitExpression(*expression.right);
		return emitBinaryOperation(expression.binaryOperator, left, right);
	}

	Id Emitter::emitBinaryOperation(BinaryOperator op, Id left, Id right) {
		const Id leftType = _builder.typeOf(left);

		if (_types.matrixInfo(leftType) || _types.matrixInfo(_builder.typeOf(right))) {
			return emitMatrixProduct(op, left, right);
		}

		if ((op == BinaryOperator::Equal || op == BinaryOperator::NotEqual)
			&& leftType == _boolType && _builder.typeOf(right) == _boolType) {
			const uint16_t opcode = op == BinaryOperator::Equal
				? spirv::OpLogicalEqual : spirv::OpLogicalNotEqual;
			return _builder.emitTyped(opcode, _boolType, { left, right });
		}

		const bool isLogical = op == BinaryOperator::LogicalAnd
			|| op == BinaryOperator::LogicalOr;

		// A comparison or logical operator yields a bool regardless of operand
		// type; an arithmetic one yields its operand type.
		const bool isComparison = !isLogical && leftType != _boolType
			&& isComparisonOperator(op);

		if (isLogical) {
			const uint16_t opcode = op == BinaryOperator::LogicalAnd
				? spirv::OpLogicalAnd : spirv::OpLogicalOr;
			if (_types.isBool(leftType) && leftType == _builder.typeOf(right)) {
				return _builder.emitTyped(opcode, leftType, { left, right });
			}

			return _builder.emitTyped(opcode, _boolType, { asCondition(left), asCondition(right) });
		}

		if (isComparison && (_types.vectorWidth(leftType) > 1 || _types.vectorWidth(_builder.typeOf(right)) > 1)) {
			return emitVectorComparison(op, left, right);
		}

		if (isComparison) {
			const ArithmeticConversion common = usualArithmeticConversion(_builder.typeOf(left), _builder.typeOf(right));
			const Id commonType = common.type;
			const bool operandsSigned = common.isSigned;
			Id leftOperand = left;
			Id rightOperand = right;

			if (_builder.typeOf(left) != commonType) {
				leftOperand = convert(left, _builder.typeOf(left), commonType);
			}
			if (_builder.typeOf(right) != commonType) {
				rightOperand = convert(right, _builder.typeOf(right), commonType);
			}

			return _builder.emitTyped(comparisonOpcode(op,
				_types.isFloat(commonType), operandsSigned), _boolType, { leftOperand, rightOperand });
		}

		// Apple promotes a bool operand to int. mslc has no such promotion, and
		// OpIAdd on a bool is a module spirv-val rejects.
		if (_types.isBool(leftType) || _types.isBool(_builder.typeOf(right))) {
			throw CompileError("an arithmetic, bitwise or comparison operator on a bool is not lowered yet; "
				"convert the bool to an int first");
		}

		return emitArithmetic(op, left, right);
	}

	// The operators + - * / % & | ^ << >> on numeric scalars and vectors, with
	// the operand rules of C and of Apple's compiler. Scalars undergo the integer
	// promotions and then the usual arithmetic conversions, so "int + float" adds
	// in float. A vector keeps its own type: a scalar beside it is converted to
	// the vector's component type, and two vectors have to be the same type.
	Id Emitter::emitArithmetic(BinaryOperator op, Id left, Id right) {
		const Id leftType = _builder.typeOf(left);
		const Id rightType = _builder.typeOf(right);

		for (const Id type: { leftType, rightType }) {
			if (_types.bitWidth(type) == 0) {
				throw CompileError("an arithmetic or bitwise operator is lowered only for "
					"numeric scalars, vectors and matrices");
			}
		}

		const bool isShift = op == BinaryOperator::ShiftLeft
			|| op == BinaryOperator::ShiftRight;
		// % on a float is rejected by arithmeticOpcode, which knows to name fmod.
		const bool needsIntegers = isShift || op == BinaryOperator::BitAnd
			|| op == BinaryOperator::BitOr || op == BinaryOperator::BitXor;
		if (needsIntegers && (_types.isFloat(leftType) || _types.isFloat(rightType))) {
			throw CompileError(std::string("operator ") + binaryOperatorSpelling(op)
				+ " needs integer operands");
		}

		const uint32_t leftWidth = _types.vectorWidth(leftType);
		const uint32_t rightWidth = _types.vectorWidth(rightType);
		const auto emit = [&](Id type, Id leftOperand, Id rightOperand) {
			return _builder.emitTyped(arithmeticOpcode(op, _types.isFloat(type),
				_types.isSignedInt(type)), type, { leftOperand, rightOperand });
		};

		if (leftWidth > 1 && rightWidth > 1) {
			if (leftWidth != rightWidth) {
				throw CompileError("the operands of an operator are vectors of different widths");
			}

			// A shift count may be any integer type; every other operator wants the
			// same vector type on both sides.
			if (!isShift && leftType != rightType) {
				throw CompileError("an operator takes two vectors of the same type");
			}
			return emit(leftType, left, right);
		}

		if (leftWidth > 1) {
			if (_types.isFloat(rightType) && !_types.isFloat(leftType)) {
				throw CompileError("a floating-point scalar cannot be combined with an integer vector");
			}
			return emit(leftType, left, broadcast(right, leftType));
		}

		if (rightWidth > 1) {
			if (isShift) {
				throw CompileError("a scalar cannot be shifted by a vector");
			}
			if (_types.isFloat(leftType) && !_types.isFloat(rightType)) {
				throw CompileError("a floating-point scalar cannot be combined with an integer vector");
			}
			return emit(rightType, broadcast(left, rightType), right);
		}

		// A shift takes its type from the promoted left operand alone, and the
		// count keeps its own.
		if (isShift) {
			const Id promoted = _types.bitWidth(leftType) < 32 ? promotedTo(1) : leftType;
			return emit(promoted, convert(left, leftType, promoted), right);
		}

		const Id common = usualArithmeticConversion(leftType, rightType).type;
		return emit(common, convert(left, leftType, common), convert(right, rightType, common));
	}

	// A comparison with a vector operand is componentwise and yields a bool vector
	// of the same width. The operand rules are those of the arithmetic operators: a
	// scalar beside a vector is converted to its component type and broadcast, and
	// two vectors have to be the same type.
	Id Emitter::emitVectorComparison(BinaryOperator op, Id left, Id right) {
		const Id leftType = _builder.typeOf(left);
		const Id rightType = _builder.typeOf(right);

		for (const Id type: { leftType, rightType }) {
			if (_types.bitWidth(type) == 0 || _types.isBool(type)) {
				throw CompileError("a comparison with a vector operand is lowered only for "
					"numeric scalars and vectors; a bool operand is not lowered yet");
			}
		}

		const uint32_t leftWidth = _types.vectorWidth(leftType);
		const uint32_t rightWidth = _types.vectorWidth(rightType);
		Id operandType = leftType;

		if (leftWidth > 1 && rightWidth > 1) {
			if (leftWidth != rightWidth) {
				throw CompileError("the operands of an operator are vectors of different widths");
			}
			if (leftType != rightType) {
				throw CompileError("an operator takes two vectors of the same type");
			}
		} else {
			operandType = leftWidth > 1 ? leftType : rightType;
			const Id scalarType = leftWidth > 1 ? rightType : leftType;
			if (_types.isFloat(scalarType) && !_types.isFloat(operandType)) {
				throw CompileError("a floating-point scalar cannot be combined with an integer vector");
			}
			if (leftWidth > 1) {
				right = broadcast(right, operandType);
			} else {
				left = broadcast(left, operandType);
			}
		}

		const Id resultType = _types.vector(ScalarKind::Bool, _types.vectorWidth(operandType));
		return _builder.emitTyped(comparisonOpcode(op, _types.isFloat(operandType),
			_types.isSignedInt(operandType)), resultType, { left, right });
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

	// "x op= v": x holds the current value and the result is what to store back.
	// The operation is the binary operator's own, applied after the usual
	// arithmetic conversions, so "int x; x += 1.5f" adds in float; the store
	// converts the sum back to int.
	Id Emitter::emitCompoundOperation(BinaryOperator op, Id current, Id value) {
		const Id targetType = _builder.typeOf(current);
		const Id valueType = _builder.typeOf(value);
		const std::string spelled = std::string(binaryOperatorSpelling(op)) + "=";

		for (const Id type: { targetType, valueType }) {
			const bool isMatrix = _types.matrixInfo(type) != nullptr;
			const bool isNumeric = _types.bitWidth(type) != 0 && type != _boolType
				&& _types.componentOf(type) != _boolType;
			if (!isMatrix && !isNumeric) {
				throw CompileError("operator " + spelled + " is lowered only for numeric scalars, "
					"vectors and matrices; a bool, a pointer or a struct operand is not lowered yet");
			}
		}

		const auto* targetMatrix = _types.matrixInfo(targetType);
		const uint32_t targetWidth = _types.vectorWidth(targetType);
		const uint32_t valueWidth = _types.vectorWidth(valueType);

		// Apple's compiler scales a matrix in place by a scalar, converted to the
		// matrix's component type, and takes a matrix only on the right of a
		// vector ("v *= m"). A matrix times a matrix is not accepted in place.
		if (targetMatrix) {
			if (_types.matrixInfo(valueType) || valueWidth > 1) {
				throw CompileError("operator " + spelled + " takes a scalar on the right of a matrix");
			}
			value = convert(value, valueType, _types.scalar(targetMatrix->scalar));
		} else if (_types.matrixInfo(valueType)) {
			if (targetWidth == 1) {
				throw CompileError("operator " + spelled + " cannot take a matrix on the right of a scalar");
			}
		}
		if (targetMatrix || _types.matrixInfo(valueType)) {
			return emitBinaryOperation(op, current, value);
		}

		const bool isShift = op == BinaryOperator::ShiftLeft || op == BinaryOperator::ShiftRight;
		const bool needsIntegers = isShift || op == BinaryOperator::Modulo
			|| op == BinaryOperator::BitAnd || op == BinaryOperator::BitOr
			|| op == BinaryOperator::BitXor;
		if (needsIntegers && (_types.isFloat(targetType) || _types.isFloat(valueType))) {
			throw CompileError("operator " + spelled + " needs integer operands");
		}

		if (targetWidth > 1) {
			// A shift count is a vector of the same width, in any integer type; every
			// other operator wants the vector's own type.
			const bool countVector = isShift && valueWidth == targetWidth;
			if (valueWidth > 1 && valueType != targetType && !countVector) {
				throw CompileError("operator " + spelled + " on a vector takes a scalar or a vector "
					"of the same type");
			}
			if (valueWidth == 1 && _types.isFloat(valueType) && !_types.isFloat(targetType)) {
				throw CompileError("operator " + spelled + " cannot take a floating-point scalar on "
					"the right of an integer vector");
			}
			return emitBinaryOperation(op, current, value);
		}

		if (valueWidth > 1) {
			throw CompileError("operator " + spelled + " cannot take a vector on the right of a scalar");
		}

		return emitBinaryOperation(op, current, value);
	}

	Id Emitter::emitCall(const Expression& expression) {
		const Expression* receiver = nullptr;
		HelperFunction* helper = findHelper(expression, receiver);
		if (!helper && expression.left->kind == ExpressionKind::Member) {
			return emitTextureCall(expression);
		}

		if (!helper && expression.left->kind != ExpressionKind::Identifier) {
			throw CompileError("only a direct function call is supported");
		}

		// A helper never has the name of a builtin, which validateHelpers refuses.
		if (helper) {
			if (returnsVoid(*helper->definition)) {
				throw CompileError("a call to \"" + helper->definition->name + "\" returns void and has "
					"no value");
			}

			return emitHelperCall(expression, *helper, receiver);
		}

		const MathBuiltin* builtin = findMathBuiltin(expression.left->name);
		if (!builtin) {
			throw CompileError("function \"" + expression.left->name + "\" is not a builtin mslc "
				"recognises, and it is not a function declared in this file");
		}

		return emitMathBuiltin(*builtin, expression.arguments);
	}

	// The helper a call names. A call through "." is a member function when the
	// receiver is a struct value, and then receiver is the expression to be passed
	// first.
	Emitter::HelperFunction* Emitter::findHelper(const Expression& call, const Expression*& receiver) {
		receiver = nullptr;
		if (call.left->kind == ExpressionKind::Member) {
			const Expression& member = *call.left;
			const std::string suffix = "::" + member.memberName;
			const auto isMember = [&](const auto& entry) {
				const std::string& name = entry.first;
				return name.size() > suffix.size()
					&& name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
			};
			const auto texture = member.left->kind == ExpressionKind::Identifier
				? _bindings.find(member.left->name) : _bindings.end();
			if ((texture != _bindings.end() && texture->second.pointeeMsl.isTexture())
				|| std::none_of(_helpers.begin(), _helpers.end(), isMember)) {
				return nullptr;
			}

			// A local names its struct, so the receiver is evaluated as the first
			// argument is; any other receiver is evaluated to learn what it is.
			std::string ownerName;
			if (texture != _bindings.end() && !texture->second.pointeeMsl.namedType.empty()) {
				ownerName = texture->second.pointeeMsl.namedType;
			} else {
				_pendingObject = emitExpression(*member.left);
				if (const std::string* owner = _types.structNameOf(_builder.typeOf(_pendingObject), nullptr)) {
					ownerName = *owner;
				}
			}
			const auto found = ownerName.empty() ? _helpers.end() : _helpers.find(ownerName + suffix);
			if (found == _helpers.end()) {
				throw CompileError("the receiver of \"." + member.memberName + "\" is not a struct with a "
					"member function of that name");
			}
			if (found->second.definition == _helper) {
				throw CompileError("recursive call: \"" + found->first + "\" calls itself; SPIR-V for Vulkan "
					"forbids recursion (Apple's compiler accepts it)");
			}
			const std::vector<Parameter>& parameters = found->second.definition->parameters;
			if (parameters.empty() || parameters[0].name != "this") {
				throw CompileError("\"" + found->first + "\" is a static member function; call it as "
					+ found->first + "(...)");
			}
			if (call.arguments.size() + 1 != parameters.size()) {
				throw CompileError("call to \"" + found->first + "\" passes " + std::to_string(call.arguments.size())
					+ " arguments, and it takes " + std::to_string(parameters.size() - 1));
			}

			rejectMemberFileScopeSamplers(*found->second.definition);
			receiver = member.left.get();
			return &found->second;
		}

		if (call.left->kind != ExpressionKind::Identifier) {
			return nullptr;
		}

		const auto found = _helpers.find(call.left->name);
		return found == _helpers.end() ? nullptr : &found->second;
	}

	bool Emitter::returnsVoid(const FunctionDecl& function) const {
		return function.returnType.scalar == ScalarKind::Void && function.returnType.namedType.empty();
	}

	// A texture or sampler parameter is a pointer to the UniformConstant variable
	// the caller's own parameter or sampler is, because Vulkan SPIR-V passes an
	// opaque object by reference. Any other parameter is the value it names.
	Id Emitter::helperParameterType(const Type& type) {
		if (type.resource == ResourceKind::None) {
			return declaredTypeOf(type);
		}

		return _types.pointer(spirv::StorageClass::UniformConstant, resourcePointee(type));
	}

	Id Emitter::resourcePointee(const Type& type) {
		if (type.textureAccess == TextureAccess::Write) return _types.uintWriteImage();
		return type.isTexture() ? _types.image(type.resource) : _types.samplerType();
	}

	// The variable behind a texture or sampler argument: it has to be a name for a
	// texture or sampler of the same kind as the parameter, since there is no
	// other way to hold one, and Apple finds no function for any other call.
	Id Emitter::resourceArgument(const Expression& argument, const Parameter& parameter,
		const std::string& function) {
		const std::string what = "the argument for parameter \"" + parameter.name + "\" of \"" + function + "\"";
		if (argument.kind != ExpressionKind::Identifier) {
			throw CompileError(what + " has to name a texture or sampler parameter, a sampler declared "
				"in the shader, or a texture or sampler parameter of the calling helper");
		}

		const Binding* binding = findResourceBinding(argument.name);
		const Type& wanted = parameter.type;
		const bool matches = binding && binding->pointeeMsl.resource == wanted.resource
			&& (!wanted.isTexture() || (binding->pointeeMsl.scalar == wanted.scalar
				&& binding->pointeeMsl.textureAccess == wanted.textureAccess));
		if (!matches) {
			throw CompileError(what + " is \"" + argument.name + "\", which is not a "
				+ typeName(wanted));
		}

		// The helper samples with an implicit lod, which Vulkan refuses through an
		// unnormalized sampler, and the helper is one function whatever it is given.
		if (binding->unnormalizedSampler) {
			throw CompileError(what + " is a coord::pixel sampler, which a helper function does not take: "
				"Vulkan forbids an implicit-lod lookup through it");
		}

		return binding->id;
	}

	// Arguments are evaluated left to right and converted to the parameter types.
	// Resource arguments retain their caller's identity, including writes.
	Id Emitter::emitHelperCall(const Expression& call, HelperFunction& helper, const Expression* receiver) {
		const FunctionDecl& definition = *helper.definition;

		if (helper.type == InvalidId) {
			helper.returnType = returnsVoid(definition) ? _voidType : declaredTypeOf(definition.returnType);
			std::vector<Id> signature { helper.returnType };
			for (const Parameter& parameter: definition.parameters) {
				helper.parameterTypes.push_back(parameter.isMutableReference()
					? _types.pointer(spirv::StorageClass::Function, declaredTypeOf(parameter.type))
					: helperParameterType(parameter.type));
				signature.push_back(helper.parameterTypes.back());
			}

			// One OpTypeFunction per distinct signature: a duplicate fails validation.
			const auto slot = _functionTypes.emplace(signature, InvalidId);
			if (slot.second) {
				slot.first->second = _builder.emitDecl(spirv::OpTypeFunction, signature);
			}

			helper.type = slot.first->second;
		}

		const auto slot = helper.ids.emplace(_entryPoint->stage, InvalidId);
		if (slot.second) {
			slot.first->second = _builder.nextId();
			_helperQueue.push_back(&helper);
		}

		std::vector<uint32_t> operands { slot.first->second };
		std::vector<std::pair<Id, Id>> copyOuts;
		const size_t first = receiver ? 1 : 0;
		rejectAliasedReferenceArguments(call, definition, first);
		if (first) {
			const Id object = _pendingObject != InvalidId ? _pendingObject : emitExpression(*receiver);
			_pendingObject = InvalidId;
			operands.push_back(convertImplicit(object, helper.parameterTypes[0]));
		}
		for (size_t i = 0; i < call.arguments.size(); ++i) {
			const Parameter& parameter = definition.parameters[first + i];
			if (parameter.type.resource != ResourceKind::None) {
				operands.push_back(resourceArgument(*call.arguments[i], parameter, definition.name));
				continue;
			}

			if (parameter.isMutableReference()) {
				operands.push_back(referenceArgument(*call.arguments[i], parameter, definition.name, i + 1, copyOuts));
				continue;
			}

			const Id value = emitExpression(*call.arguments[i]);
			try {
				operands.push_back(convertImplicit(value, helper.parameterTypes[first + i]));
			} catch (const CompileError& error) {
				throw CompileError("argument " + std::to_string(i + 1) + " of the call to \""
					+ definition.name + "\" cannot be converted to the parameter type "
					+ typeName(definition.parameters[first + i].type) + ": " + error.what());
			}
		}

		const Id result = _builder.emitTyped(spirv::OpFunctionCall, helper.returnType, operands);
		for (const auto& [place, temporary]: copyOuts) {
			const Id type = valueTypeAt(place);
			_builder.emit(spirv::OpStore, { place, loadFrom(temporary, type) });
		}
		return result;
	}

	// An element, a member, or a reference local bound to one is passed through a
	// temporary copied back after the call, so a second reference argument naming
	// the same storage would have its write overwritten by the stale copy. Two
	// bare variables are the same pointer and alias as C++ does.
	void Emitter::rejectAliasedReferenceArguments(const Expression& call, const FunctionDecl& definition,
		size_t first) const {
		struct Passed { std::string root; bool temporary; };
		std::vector<Passed> passed;
		for (size_t i = 0; i < call.arguments.size(); ++i) {
			if (!definition.parameters[first + i].isMutableReference()) {
				continue;
			}
			const Expression& argument = *call.arguments[i];
			if (argument.kind != ExpressionKind::Identifier && argument.kind != ExpressionKind::Index
				&& argument.kind != ExpressionKind::Member) {
				continue;
			}
			const Expression& root = storeRoot(argument);
			if (root.kind != ExpressionKind::Identifier) {
				continue;
			}
			const auto bound = _bindings.find(root.name);
			passed.push_back({ root.name, argument.kind != ExpressionKind::Identifier
				|| (bound != _bindings.end() && bound->second.isAddress) });
		}

		for (size_t i = 0; i < passed.size(); ++i) {
			for (size_t j = i + 1; j < passed.size(); ++j) {
				const bool sameRoot = passed[i].root == passed[j].root;
				const bool viaAddress = (passed[i].temporary || passed[j].temporary)
					&& (_bindings.count(passed[i].root) && _bindings.at(passed[i].root).isAddress
						|| _bindings.count(passed[j].root) && _bindings.at(passed[j].root).isAddress);
				if ((sameRoot && (passed[i].temporary || passed[j].temporary)) || viaAddress) {
					throw CompileError("the call to \"" + definition.name + "\" passes the same variable "
						"through two reference parameters, one of them as a member or element; that is "
						"not lowered yet");
				}
			}
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
		if (argument.kind == ExpressionKind::Member && !structOf(*argument.left)) {
			throw CompileError(what + " is a component of a vector, which a reference parameter cannot "
				"name yet");
		}

		const Expression& root = storeRoot(argument);
		if (root.kind == ExpressionKind::Identifier) {
			const auto binding = _bindings.find(root.name);
			if (binding != _bindings.end() && binding->second.readOnly) {
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
		if (valueTypeAt(address) != declaredTypeOf(parameter.type)) {
			throw CompileError(what + " is not the type of parameter \"" + parameter.name + "\", "
				+ typeName(parameter.type) + "; a conversion would make a temporary");
		}

		// A function parameter has to be a pointer to a variable, which an element
		// or a member of one is not, so those go through a temporary copied back
		// after the call.
		if (argument.kind == ExpressionKind::Identifier) {
			const auto named = _bindings.find(argument.name);
			if (named != _bindings.end() && !named->second.isAddress) {
				return address;
			}
		}

		const Id type = declaredTypeOf(parameter.type);
		const Id temporary = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Function, type),
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });
		_builder.emit(spirv::OpStore, { temporary, loadFrom(address, type) });
		copyOuts.emplace_back(address, temporary);
		return temporary;
	}

	// A parameter is a value the body may assign to, as in C++, and the caller's
	// copy must not change, so each named one is copied into a variable of the
	// helper's own and the body reads and writes that.
	void Emitter::emitHelper(const HelperFunction& helper) {
		const FunctionDecl& definition = *helper.definition;

		_helper = &definition;
		_bindings.clear();
		_terminated = false;
		_controlDepth = 0;

		_builder.setSection(spirv::Section::Functions);
		_builder.emitDeclTypedAt(spirv::OpFunction, helper.returnType, helper.ids.at(_entryPoint->stage),
			{ kFunctionControlNone, helper.type });

		std::vector<Id> values;
		for (const Id type: helper.parameterTypes) {
			values.push_back(_builder.emitTyped(spirv::OpFunctionParameter, type, { }));
		}

		beginBlock(_builder.nextId());
		for (size_t i = 0; i < values.size(); ++i) {
			const Parameter& parameter = definition.parameters[i];
			if (parameter.name.empty()) {
				continue;
			}

			if (parameter.type.resource != ResourceKind::None) {
				Binding bound;
				bound.id = values[i];
				bound.isPointer = true;
				bound.pointeeType = resourcePointee(parameter.type);
				bound.storageClass = spirv::StorageClass::UniformConstant;
				bound.pointeeMsl = parameter.type;
				_bindings[parameter.name] = bound;
				continue;
			}

			if (parameter.isMutableReference()) {
				bindLocal(parameter.name, values[i], declaredTypeOf(parameter.type), parameter.type);
				continue;
			}

			const Id type = helper.parameterTypes[i];
			const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
				_types.pointer(spirv::StorageClass::Function, type),
				{ static_cast<uint32_t>(spirv::StorageClass::Function) });
			_builder.emit(spirv::OpStore, { variable, values[i] });
			bindLocal(parameter.name, variable, type, parameter.type);
		}

		emitFunctionBody(*definition.body);

		// A path that reaches the end of a function returning a value is undefined
		// in C++ and Apple only warns for it, so it is not made up a value. A block
		// left open by a structured if whose branches all return is dead, and this
		// terminates that too.
		if (!_terminated) {
			terminate(returnsVoid(definition) ? spirv::OpReturn : spirv::OpUnreachable, { });
		}
		_builder.emit(spirv::OpFunctionEnd, { });

		_helper = nullptr;
	}

	void Emitter::emitHelperReturn(const Statement& statement) {
		const std::string quoted = "\"" + _helper->name + "\"";

		if (returnsVoid(*_helper)) {
			if (statement.expression) {
				throw CompileError(quoted + " returns void, so its return cannot carry a value");
			}

			terminate(spirv::OpReturn, { });
			return;
		}

		if (!statement.expression) {
			throw CompileError(quoted + " returns " + typeName(_helper->returnType)
				+ ", so a return needs a value");
		}

		const Id value = emitExpression(*statement.expression);
		terminate(spirv::OpReturnValue, { convertImplicit(value, declaredTypeOf(_helper->returnType)) });
	}

	Id Emitter::emitMathBuiltin(const MathBuiltin& builtin,
		const std::vector<ExpressionPtr>& arguments) {

		const std::string name = builtin.name;
		if (arguments.size() != builtin.arity) {
			throw CompileError(name + " takes " + std::to_string(builtin.arity) + " argument"
				+ (builtin.arity == 1 ? "" : "s") + ", and this call passes "
				+ std::to_string(arguments.size()));
		}

		if (builtin.shape == MathShape::Fwidth && _entryPoint->stage != Stage::Fragment) {
			throw CompileError("fwidth is only available in fragment functions");
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

		if (builtin.shape == MathShape::Fwidth) {
			const Id type = _builder.typeOf(values[0]);
			if (!_types.isFloat(type) || _types.bitWidth(type) != 32) {
				throw CompileError("fwidth takes float scalars or vectors, half is not supported yet");
			}
			return _builder.emitTyped(spirv::OpFwidth, type, { values[0] });
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
			_types.scalar(folded.scalar), constantWords(folded));
	}

	// The literal words of a scalar constant of its own kind: one for anything up
	// to 32 bits and two, low then high, for a 64-bit integer. A float is narrowed
	// to its own width here, since it was folded as a double and OpConstant takes
	// the bits of the type it declares.
	std::vector<uint32_t> Emitter::constantWords(const FoldedConstant& folded) {
		const ScalarKind kind = folded.scalar;

		if (kind == ScalarKind::Double) {
			throw CompileError("a constant of " + std::string(scalarKindName(kind))
				+ " is not lowered yet");
		}

		// A half's literal is its own 16 bits, not the low half of a float's.
		if (kind == ScalarKind::Half) {
			return { halfBitsOf(folded.number) };
		}

		if (kind == ScalarKind::Float) {
			const auto narrowed = static_cast<float>(folded.number);
			uint32_t bits = 0;
			static_assert(sizeof(bits) == sizeof(narrowed), "float is not 32 bits");
			std::memcpy(&bits, &narrowed, sizeof(bits));
			return { bits };
		}

		std::vector<uint32_t> words { static_cast<uint32_t>(folded.integer) };
		if (scalarBitWidth(kind) == 64) {
			words.push_back(static_cast<uint32_t>(folded.integer >> 32));
		}

		return words;
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
				foldInitializer(decl.fields[i].type, *element.value, true)));
		}
	}

	// An initialiser that is not a list, so a single value. Only what can be
	// worked out from the declarations before it is folded; anything else is
	// reported, since a constant has to be constant.
	FoldedConstant Emitter::foldExpression(const Expression& expression) {
		FoldedConstant folded;

		switch (expression.kind) {
			case ExpressionKind::IntLiteral:
				folded.scalar = expression.intKind;
				folded.integer = normalizeInteger(expression.intKind, expression.intValue);
				return folded;

			case ExpressionKind::FloatLiteral:
				if (expression.floatIsHalf) {
					folded.scalar = ScalarKind::Half;
					folded.number = roundedToKind(ScalarKind::Half, expression.floatValue);
					return folded;
				}

				folded.scalar = ScalarKind::Float;
				folded.number = roundedToKind(ScalarKind::Float, expression.floatValue);
				return folded;

			case ExpressionKind::BoolLiteral:
				folded.scalar = ScalarKind::Bool;
				folded.boolean = expression.boolValue;
				return folded;

			case ExpressionKind::Identifier: {
				const auto local = _bindings.find(expression.name);
				if (local != _bindings.end()) {
					const auto value = _localFolded.find(local->second.id);
					if (value != _localFolded.end()) { return value->second; }
					if (!local->second.folded) {
						throw CompileError("a local in this initializer is not a supported constant expression");
					}
					return *local->second.folded;
				}
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
		FoldedConstant operand = foldExpression(*expression.left);

		if (operand.isComposite) {
			throw CompileError("a composite constant cannot have a unary operator applied to it");
		}

		const UnaryOperator op = expression.unaryOperator;
		const bool isBool = operand.scalar == ScalarKind::Bool;
		const bool isFloat = isFloatKind(operand.scalar);

		if (op == UnaryOperator::Not) {
			FoldedConstant folded;
			folded.scalar = ScalarKind::Bool;
			folded.boolean = isBool ? !operand.boolean : isFloat ? operand.number == 0.0 : operand.integer == 0;
			return folded;
		}

		if (op != UnaryOperator::Plus && op != UnaryOperator::Negate && op != UnaryOperator::BitNot) {
			throw CompileError("this unary operator is recognised but not folded yet");
		}

		if (isBool) {
			throw CompileError("a unary operator on a bool is not folded; convert the bool to an int first");
		}

		if (isFloat) {
			if (op == UnaryOperator::BitNot) {
				throw CompileError("operator ~ needs an integer operand");
			}

			if (op == UnaryOperator::Negate) {
				operand.number = -operand.number;
			}

			return operand;
		}

		// The operand is promoted first, so -c for a char is an int, and the
		// result wraps in its own width like the instruction the lowering emits.
		operand.scalar = promotedKind(operand.scalar);
		if (op == UnaryOperator::Negate) {
			if (isSignedInteger(operand.scalar)
				&& operand.integer == normalizeInteger(operand.scalar, uint64_t{ 1 } << (mappingFor(operand.scalar).width - 1))) {
				_foldDomainError = true;
			}
			operand.integer = normalizeInteger(operand.scalar, uint64_t{ 0 } - operand.integer);
		} else if (op == UnaryOperator::BitNot) {
			operand.integer = normalizeInteger(operand.scalar, ~operand.integer);
		}

		return operand;
	}

	FoldedConstant Emitter::foldBinary(const Expression& expression) {
		// && and || do not evaluate the right operand once the left decides the
		// result, so an operation in it that no constant expression holds is not
		// reached either.
		if (expression.binaryOperator == BinaryOperator::LogicalAnd
			|| expression.binaryOperator == BinaryOperator::LogicalOr) {
			const auto truth = [](const FoldedConstant& value) {
				if (value.isComposite) {
					throw CompileError("a composite constant cannot be an operand of a binary operator");
				}
				return value.scalar == ScalarKind::Bool ? value.boolean
					: isFloatKind(value.scalar) ? value.number != 0.0 : value.integer != 0;
			};
			const bool isAnd = expression.binaryOperator == BinaryOperator::LogicalAnd;
			const bool decided = truth(foldExpression(*expression.left)) != isAnd;
			FoldedConstant folded;
			folded.scalar = ScalarKind::Bool;
			folded.boolean = decided ? !isAnd : truth(foldExpression(*expression.right));
			return folded;
		}

		const FoldedConstant left = foldExpression(*expression.left);
		const FoldedConstant right = foldExpression(*expression.right);

		if (left.isComposite || right.isComposite) {
			throw CompileError("a composite constant cannot be an operand of a binary operator");
		}

		if (left.scalar == ScalarKind::Bool || right.scalar == ScalarKind::Bool) {
			throw CompileError("an arithmetic or bitwise operator on a bool is not folded; "
				"convert the bool to an int first");
		}

		const BinaryOperator op = expression.binaryOperator;

		if (isFloatKind(left.scalar) || isFloatKind(right.scalar)) {
			// A float beats a half, and either beats an integer, as in C.
			FoldedConstant folded;
			folded.scalar = left.scalar == ScalarKind::Float || right.scalar == ScalarKind::Float
				? ScalarKind::Float : ScalarKind::Half;
			const auto valueOf = [&](const FoldedConstant& operand) {
				return isFloatKind(operand.scalar) ? operand.number
					: integerAsKind(folded.scalar, operand.scalar, operand.integer);
			};

			const double l = valueOf(left);
			const double r = valueOf(right);
			switch (op) {
				case BinaryOperator::Add: folded.number = l + r; break;
				case BinaryOperator::Subtract: folded.number = l - r; break;
				case BinaryOperator::Multiply: folded.number = l * r; break;
				// A float division by zero is infinity, which is what the language
				// says, so it is not treated as a mistake here.
				case BinaryOperator::Divide:
					if (r == 0.0) { _foldDomainError = true; }
					folded.number = l / r;
					break;
				default:
					throw CompileError("this operator is recognised but not folded yet");
			}

			if (std::isnan(folded.number)) { _foldDomainError = true; }
			folded.number = roundedToKind(folded.scalar, folded.number);
			return folded;
		}

		const bool isShift = op == BinaryOperator::ShiftLeft || op == BinaryOperator::ShiftRight;

		// A shift takes its type from the promoted left operand alone, and a count
		// of its own type that has to lie within that type's width.
		if (isShift) {
			FoldedConstant folded;
			folded.scalar = promotedKind(left.scalar);
			const uint32_t width = mappingFor(folded.scalar).width;
			const bool negative = isSignedInteger(right.scalar) && static_cast<int64_t>(right.integer) < 0;
			if (negative || right.integer >= width) {
				_foldDomainError = true;
				throw CompileError("a shift of a constant is by a count outside 0 to "
					+ std::to_string(width - 1));
			}

			const uint64_t count = right.integer;
			if (op == BinaryOperator::ShiftLeft) {
				folded.integer = normalizeInteger(folded.scalar, left.integer << count);
			} else if (isSignedInteger(folded.scalar)) {
				folded.integer = static_cast<uint64_t>(static_cast<int64_t>(left.integer) >> count);
			} else {
				folded.integer = left.integer >> count;
			}

			return folded;
		}

		FoldedConstant folded;
		folded.scalar = commonIntegerKind(promotedKind(left.scalar), promotedKind(right.scalar));
		const uint64_t l = normalizeInteger(folded.scalar, left.integer);
		const uint64_t r = normalizeInteger(folded.scalar, right.integer);
		const bool isSigned = isSignedInteger(folded.scalar);

		uint64_t value = 0;
		if (isSigned && (op == BinaryOperator::Add || op == BinaryOperator::Subtract
			|| op == BinaryOperator::Multiply)) {
			// The exact result has to lie in the type; a signed overflow is undefined
			// in C++ and so no constant expression.
			const auto sl = static_cast<int64_t>(l);
			const auto sr = static_cast<int64_t>(r);
			int64_t exact = 0;
			bool overflow = op == BinaryOperator::Add ? __builtin_add_overflow(sl, sr, &exact)
				: op == BinaryOperator::Subtract ? __builtin_sub_overflow(sl, sr, &exact)
				: __builtin_mul_overflow(sl, sr, &exact);
			const uint32_t width = mappingFor(folded.scalar).width;
			if (!overflow && width < 64) {
				const int64_t most = (int64_t{ 1 } << (width - 1)) - 1;
				overflow = exact > most || exact < -most - 1;
			}
			if (overflow) { _foldDomainError = true; }
		}
		switch (op) {
			case BinaryOperator::Add: value = l + r; break;
			case BinaryOperator::Subtract: value = l - r; break;
			case BinaryOperator::Multiply: value = l * r; break;
			case BinaryOperator::Modulo:
			case BinaryOperator::Divide:
				if (r == 0) {
					_foldDomainError = true;
					throw CompileError("an integer constant divides by zero");
				}

				if (isSigned) {
					const auto sl = static_cast<int64_t>(l);
					const auto sr = static_cast<int64_t>(r);
					// The one quotient that does not fit is the most negative value
					// over -1, which wraps to itself, with a remainder of 0.
					const uint64_t most = normalizeInteger(folded.scalar,
						uint64_t{ 1 } << (mappingFor(folded.scalar).width - 1));
					const bool overflows = sr == -1 && l == most;
					if (overflows) { _foldDomainError = true; }
					value = overflows ? (op == BinaryOperator::Divide ? l : 0)
						: static_cast<uint64_t>(op == BinaryOperator::Divide ? sl / sr : sl % sr);
				} else {
					value = op == BinaryOperator::Divide ? l / r : l % r;
				}
				break;
			case BinaryOperator::BitAnd: value = l & r; break;
			case BinaryOperator::BitOr: value = l | r; break;
			case BinaryOperator::BitXor: value = l ^ r; break;
			default:
				throw CompileError("this operator is recognised but not folded yet");
		}

		folded.integer = normalizeInteger(folded.scalar, value);
		return folded;
	}

	// Folds an initialiser of the given declared type. A list of values for a
	// vector or a struct is a composite, and anything else is a scalar, so the
	// declared type is what says which the source wrote.
	FoldedConstant Emitter::foldInitializer(const Type& type, const Expression& initializer, bool braced) {
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

			if (braced) {
				requireNotNarrowing(folded, type.scalar);
			}

			// The declared type is what the constant is. An integer literal may
			// initialise a float, since that is a widening; the other direction would
			// have to round, which is not something to do silently.
			if (isFloatKind(folded.scalar) && !isFloatKind(type.scalar)) {
				throw CompileError("a float cannot initialise a "
					+ std::string(scalarKindName(type.scalar)));
			}

			return convertConstant(folded, type.scalar);
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
				folded.parts.push_back(emitConstant(foldInitializer(component, *element.value, true)));
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
		_foldDomainError = false;
		const FoldedConstant folded = foldInitializer(declaration.type, *declaration.initializer);
		if (_foldDomainError && declaration.type.isConstexpr) {
			throw CompileError("constexpr variable \"" + declaration.name + "\" must be initialized by a "
				"constant expression");
		}
		_foldDomainError = false;
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
			if (global.sampler) {
				_fileScopeSamplers[global.name] = &global;
				continue;
			}
			_builder.setSection(spirv::Section::TypesGlobals);
			declareGlobalConstant(global);
		}
	}

	Id Emitter::emitExpression(const Expression& expression) {
		switch (expression.kind) {
			case ExpressionKind::IntLiteral: {
				// The literal's own type, which the parser took from its spelling:
				// "-1" is a negated int and "3000000000" a long, so neither wraps.
				// Emitting every literal as %uint made "-1" OpSNegate %uint, which
				// is 4294967295 as a float3 component, and as %int made
				// "4294967295u / 3u" an OpSDiv, because emitBinary takes the opcode
				// from the left operand's type.
				const Id type = _types.scalar(expression.intKind);
				std::vector<uint32_t> words { static_cast<uint32_t>(expression.intValue) };
				if (scalarBitWidth(expression.intKind) == 64) {
					words.push_back(static_cast<uint32_t>(expression.intValue >> 32));
				}
				return _builder.emitDeclTyped(spirv::OpConstant, type, words);
			}

			case ExpressionKind::FloatLiteral: {
				if (expression.floatIsHalf) {
					return _builder.emitDeclTyped(spirv::OpConstant, _types.scalar(ScalarKind::Half),
						{ halfBitsOf(expression.floatValue) });
				}

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
			case ExpressionKind::Conditional: return emitConditional(expression);
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

	// A parameter taken by value (a stage input, a builtin) lives in the thread
	// address space, which is also what it gets with none written. Apple rejects
	// any other, by name, so "device In in [[stage_in]]" is a mistake to report
	// rather than a spelling to accept.
	void requireThreadAddressSpace(const Parameter& parameter, const char* what) {
		if (parameter.type.addressSpace != AddressSpace::None
			&& parameter.type.addressSpace != AddressSpace::Thread) {
			throw CompileError("parameter \"" + parameter.name + "\" takes " + what + " by value, so it "
				"cannot be in the " + addressSpaceName(parameter.type.addressSpace) + " address space");
		}
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
			const bool isTexture = parameter.type.isTexture();
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
				if (_entryPoint->stage == Stage::Kernel) {
					throw CompileError("[[stage_in]] on a kernel function is not lowered (parameter \""
						+ parameter.name + "\"); mslc lowers it on a vertex or a fragment function");
				}

				if (_stageIn) {
					throw CompileError("a function takes one [[stage_in]] parameter, and \""
						+ _entryPoint->name + "\" has a second (\"" + parameter.name + "\")");
				}

				if (parameter.isReference) {
					throw CompileError("[[stage_in]] parameter \"" + parameter.name + "\" is a "
						"reference, which Apple's compiler does not allow; take the struct by value");
				}

				requireThreadAddressSpace(parameter, "[[stage_in]]");

				const StructDecl* decl = structValue(parameter.type);
				if (!decl) {
					throw CompileError("[[stage_in]] parameter \"" + parameter.name + "\" is a "
						+ typeName(parameter.type) + "; mslc lowers a [[stage_in]] struct and no "
							"other type");
				}

				_stageIn = &parameter;
				_stageInputs = _entryPoint->stage == Stage::Vertex
					? declareVertexAttributes(*decl)
					: declareStageStruct(*decl, spirv::StorageClass::Input);
				continue;
			}

			if (parameter.attributes.builtin) {
				requireThreadAddressSpace(parameter, "a builtin");
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
				binding.readOnly = parameter.type.isConst;
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

		// Apple takes a threadgroup pointer on a kernel only as an argument of its
		// own, with no [[buffer]], and takes thread and an unwritten address space
		// nowhere.
		const AddressSpace space = parameter.type.addressSpace;
		const bool threadgroupArgument = space == AddressSpace::Threadgroup
			&& !parameter.attributes.bufferIndex && _entryPoint->stage == Stage::Kernel;
		if (space == AddressSpace::Threadgroup && !parameter.attributes.bufferIndex
			&& _entryPoint->stage != Stage::Kernel) {
			throw CompileError("parameter \"" + parameter.name + "\" is a threadgroup parameter; "
				"threadgroup parameters are only supported on kernel functions");
		}
		if (space == AddressSpace::None) {
			throw CompileError("parameter \"" + parameter.name + "\": pointer parameter needs an "
				"explicit address space (device or constant)");
		}
		if (space != AddressSpace::Device && space != AddressSpace::Constant && !threadgroupArgument) {
			throw CompileError("parameter \"" + parameter.name + "\" needs a device or constant "
				"address space to be a buffer binding, not " + std::string(addressSpaceName(space)));
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
		reserveFileScopeSamplers();
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
			const bool isTexture = parameter->type.isTexture();
			const auto& declared = isTexture ? parameter->attributes.textureIndex
				: parameter->attributes.samplerIndex;
			if (declared) {
				taken[isTexture ? 0 : 1].insert(*declared);
			}
		}

		for (const auto& [index, parameter]: resources) {
			const bool isTexture = parameter->type.isTexture();
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
			const Id imageType = resourcePointee(entry.parameter->type);
			const Id variable = declareDescriptorVariable(imageType, binding);
			if (entry.parameter->type.textureAccess == TextureAccess::Write) {
				_builder.emit(spirv::OpDecorate, { variable, static_cast<uint32_t>(spirv::Decoration::NonReadable) });
			}

			Binding bound;
			bound.id = variable;
			bound.isPointer = true;
			bound.pointeeType = imageType;
			bound.storageClass = spirv::StorageClass::UniformConstant;
			bound.pointeeMsl = entry.parameter->type;
			_bindings[entry.parameter->name] = bound;

			addReflectionEntry("{ \"kind\": \"Texture\", \"metal_index\": "
				+ std::to_string(entry.metalIndex)
				+ ", \"descriptor\": " + descriptorJson(binding)
				+ ", \"texture_access\": \""
				+ (entry.parameter->type.textureAccess == TextureAccess::Write ? "Write" : "Sample") + "\""
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
			reserveSampler(*statement.declaration);
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
		for (const SwitchCase& kase: statement.switchCases) {
			for (const StatementPtr& child: kase.body) {
				reserveLocalSamplers(*child, used);
			}
		}
	}

	void Emitter::reserveSampler(const VariableDeclaration& declaration) {
		const bool known = std::any_of(_embeddedSamplers.begin(), _embeddedSamplers.end(),
			[&](const EmbeddedSampler& other) { return other.state == *declaration.sampler; });
		if (known) {
			return;
		}

		const uint32_t binding = _nextBinding++;
		_embeddedSamplers.push_back({ *declaration.sampler,
			declareDescriptorVariable(_types.samplerType(), binding) });

		addReflectionEntry("{ \"kind\": \"Sampler\", \"descriptor\": " + descriptorJson(binding)
			+ ", \"embedded_sampler\": " + std::to_string(_embeddedSamplers.size() - 1)
			+ ", \"name\": \"" + declaration.name + "\" }");
	}

	void Emitter::rejectMemberFileScopeSamplers(const FunctionDecl& member) const {
		std::set<const FunctionDecl*> seen { &member };
		std::vector<const FunctionDecl*> pending { &member };
		while (!pending.empty()) {
			const FunctionDecl* function = pending.back();
			pending.pop_back();
			ScopeStack scopes(1);
			for (const Parameter& parameter: function->parameters) {
				scopes.back().insert(parameter.name);
			}
			std::set<std::string> names;
			collectUnshadowed(function->body.get(), scopes, names);
			for (const VariableDeclaration& global: _unit.globals) {
				if (global.sampler && names.count(global.name)) {
					throw CompileError("member helper \"" + member.name + "\" names a file-scope sampler, "
						"pass the sampler to it as an argument");
				}
			}
			std::vector<const Expression*> calls;
			collectCalls(*function->body, calls);
			for (const Expression* call: calls) {
				if (call->left->kind != ExpressionKind::Identifier) {
					continue;
				}
				const auto helper = _helpers.find(call->left->name);
				if (helper != _helpers.end() && seen.insert(helper->second.definition).second) {
					pending.push_back(helper->second.definition);
				}
			}
		}
	}

	// A file-scope sampler is reserved, once the entry point's own are, if the body
	// names it where no parameter or local of that name hides it. They go in the
	// order the file declares them. Which embedded samplers an entry point lists is
	// per entry point, so a sampler two of them name is listed by each.
	void Emitter::reserveFileScopeSamplers() {
		std::set<const VariableDeclaration*> named;
		const auto scan = [&](const FunctionDecl& function) {
			ScopeStack scopes(1);
			for (const Parameter& parameter: function.parameters) {
				scopes.back().insert(parameter.name);
			}
			std::set<std::string> names;
			collectUnshadowed(function.body.get(), scopes, names);

			const auto declaredAbove = _unit.globals.begin() + function.globalsBefore;
			bool any = false;
			for (auto global = _unit.globals.begin(); global != declaredAbove; ++global) {
				if (global->sampler && names.count(global->name)) {
					named.insert(&*global);
					any = true;
				}
			}
			return any;
		};

		scan(*_entryPoint);

		// A helper naming a file-scope sampler is owned by one entry point.
		std::set<const FunctionDecl*> seen { _entryPoint };
		std::vector<const FunctionDecl*> pending { _entryPoint };
		while (!pending.empty()) {
			const FunctionDecl* function = pending.back();
			pending.pop_back();

			std::vector<const Expression*> calls;
			collectCalls(*function->body, calls);
			for (const Expression* call: calls) {
				if (call->left->kind != ExpressionKind::Identifier) {
					continue;
				}
				const auto helper = _helpers.find(call->left->name);
				if (helper == _helpers.end() || !seen.insert(helper->second.definition).second) {
					continue;
				}

				pending.push_back(helper->second.definition);
				if (scan(*helper->second.definition)) {
					const auto owner = _samplerHelperOwner.emplace(helper->first, _entryPoint);
					if (owner.first->second != _entryPoint) {
						throw CompileError("helper function \"" + helper->first + "\" names a file-scope "
							"sampler, and entry points \"" + owner.first->second->name + "\" and \""
							+ _entryPoint->name + "\" both reach it; helper file-scope samplers are "
							"owned by one entry point, so pass the sampler to it as an argument");
					}
				}
			}
		}

		// They go in the order the file declares them.
		for (const VariableDeclaration& global: _unit.globals) {
			if (named.count(&global)) {
				reserveSampler(global);
				_fileScopeSamplerBindings[global.name] = embeddedSamplerBinding(global);
			}
		}
	}

	Binding Emitter::embeddedSamplerBinding(const VariableDeclaration& declaration) const {
		const auto found = std::find_if(_embeddedSamplers.begin(), _embeddedSamplers.end(),
			[&](const EmbeddedSampler& other) { return other.state == *declaration.sampler; });

		Binding bound;
		bound.id = found->variable;
		bound.isPointer = true;
		bound.pointeeType = _types.samplerType();
		bound.storageClass = spirv::StorageClass::UniformConstant;
		bound.pointeeMsl = declaration.type;
		bound.unnormalizedSampler = !declaration.sampler->normalizedCoordinates;
		return bound;
	}

	void Emitter::declareLocalSampler(const VariableDeclaration& declaration) {
		const bool reserved = std::any_of(_embeddedSamplers.begin(), _embeddedSamplers.end(),
			[&](const EmbeddedSampler& other) { return other.state == *declaration.sampler; });
		if (!reserved) {
			return;  // never named after its declaration, so Apple records no state for it
		}

		_bindings[declaration.name] = embeddedSamplerBinding(declaration);
	}

	// A half texture is sampled as a float image and narrowed here, because the
	// image type has a float sampled type whatever the texture's component is.
	Id Emitter::texturePixels(const Binding& texture, Id sampled) {
		if (texture.pointeeMsl.scalar != ScalarKind::Half) {
			return sampled;
		}

		return convert(sampled, _builder.typeOf(sampled), _types.vector(ScalarKind::Half, 4));
	}

	const Binding& Emitter::textureReceiver(const Expression& call) {
		const Expression& member = *call.left;
		if (member.left->kind != ExpressionKind::Identifier) {
			throw CompileError("the receiver of \"." + member.memberName + "\" has to be a texture parameter");
		}

		const auto it = _bindings.find(member.left->name);
		if (it == _bindings.end() || !it->second.pointeeMsl.isTexture()) {
			throw CompileError("\"" + member.left->name + "\" is not a texture parameter, so "
				"\"." + member.memberName + "\" is not a call mslc lowers");
		}

		return it->second;
	}

	void Emitter::emitTextureWrite(const Expression& call, const Binding& texture) {
		if (texture.pointeeMsl.textureAccess != TextureAccess::Write) {
			throw CompileError("the texture method \"write\" is not lowered yet on sampled textures");
		}
		if (call.arguments.size() != 2) {
			throw CompileError("texture write takes exactly a uint scalar and a uint2 coordinate");
		}
		const Id value = emitExpression(*call.arguments[0]);
		const Id coordinate = emitExpression(*call.arguments[1]);
		if (_builder.typeOf(value) != _uintType
			|| _builder.typeOf(coordinate) != _types.vector(ScalarKind::UInt, 2)) {
			throw CompileError("texture write takes exactly a uint scalar and a uint2 coordinate");
		}
		const Id texel = broadcast(value, _types.vector(ScalarKind::UInt, 4));
		const Id image = loadFrom(texture.id, texture.pointeeType);
		_builder.emit(spirv::OpImageWrite, { image, coordinate, texel });
	}

	Id Emitter::emitTextureCall(const Expression& call) {
		const Binding& texture = textureReceiver(call);
		const std::string& method = call.left->memberName;
		if (method == "write") {
			throw CompileError("texture write returns void and has no value");
		}
		if (texture.pointeeMsl.textureAccess == TextureAccess::Write) {
			if (method != "get_width" || !call.arguments.empty()) {
				throw CompileError("write-only textures lower only get_width() and statement write(uint, uint2)");
			}
			return emitTextureSize(call, texture, 0);
		}
		if (method == "sample") {
			return emitTextureSample(call, texture);
		}
		if (texture.pointeeMsl.resource == ResourceKind::TextureCube) {
			throw CompileError("the texturecube method \"" + method + "\" is not lowered yet; mslc "
				"lowers sample on a texturecube");
		}
		if (method == "read") {
			return emitTextureRead(call, texture);
		}
		if (method == "get_width" || method == "get_height") {
			return emitTextureSize(call, texture, method == "get_width" ? 0 : 1);
		}
		if (method == "get_depth" && texture.pointeeMsl.resource == ResourceKind::Texture3D) {
			return emitTextureSize(call, texture, 2);
		}
		if (method == "get_array_size" && texture.pointeeMsl.resource == ResourceKind::Texture2DArray) {
			if (!call.arguments.empty()) {
				throw CompileError("get_array_size takes no arguments; the layer count has no mip level");
			}
			return emitTextureSize(call, texture, 2);
		}

		throw CompileError("the texture method \"" + method + "\" is not lowered yet; mslc lowers "
			"sample, read, get_width, get_height, get_depth on a texture3d and get_array_size on a "
			"texture2d_array");
	}

	// The components of the coordinate argument of sample and read on an image of
	// this kind. A texture2d_array takes its layer as a separate argument, so its
	// coordinate is two wide and its size three; a cube's coordinate is a
	// direction, three wide, and its size two.
	static uint32_t textureCoordinateWidth(ResourceKind kind) {
		switch (kind) {
			case ResourceKind::Texture2D: return 2;
			case ResourceKind::Texture2DArray: return 2;
			case ResourceKind::TextureCube: return 3;
			case ResourceKind::Texture3D: return 3;
			case ResourceKind::None:
			case ResourceKind::Sampler: break;
		}
		throw CompileError(std::string("\"") + resourceKindName(kind) + "\" has no texel coordinate");
	}

	static uint32_t textureSizeWidth(ResourceKind kind) {
		return kind == ResourceKind::Texture2DArray ? 3
			: kind == ResourceKind::TextureCube ? 2 : textureCoordinateWidth(kind);
	}

	// A texture2d_array coordinate of `component` with its layer appended as the
	// last component, which is where an arrayed image instruction takes it. The
	// layer is a uint in Metal, so an int wraps to uint before a sample sees it.
	Id Emitter::appendArrayLayer(Id coordinate, ScalarKind component, const Expression& layer,
		const std::string& callName) {
		const Id value = emitExpression(layer);
		const Id valueType = _builder.typeOf(value);
		if (_types.vectorWidth(valueType) != 1 || _types.isFloat(valueType) || _types.bitWidth(valueType) < 8) {
			throw CompileError("the array index of " + callName + " has to be an integer");
		}

		const Id index = convert(value, valueType, _uintType);
		return _builder.emitTyped(spirv::OpCompositeConstruct,
			_types.vector(component, _types.vectorWidth(_builder.typeOf(coordinate)) + 1),
			{ coordinate, convert(index, _uintType, _types.scalar(component)) });
	}

	// sample(sampler, float2 coordinate [, level(lod)]), a float3 coordinate on a
	// texturecube or texture3d, or a float2 coordinate and a uint layer on a
	// texture2d_array. A fragment function takes
	// the level from the derivatives, which is OpImageSampleImplicitLod; a vertex
	// or kernel function has none, so it samples level 0 with an explicit lod,
	// and so does a sampler with pixel coordinates, which Vulkan does not allow
	// an implicit lod on.
	Id Emitter::emitTextureSample(const Expression& call, const Binding& texture) {
		const auto& arguments = call.arguments;
		const ResourceKind kind = texture.pointeeMsl.resource;
		const bool arrayed = kind == ResourceKind::Texture2DArray;
		const size_t fixed = arrayed ? 3 : 2;
		if (arguments.size() < fixed || arguments.size() > fixed + 1) {
			throw CompileError(std::string("sample takes a sampler, a coordinate")
				+ (arrayed ? ", an array index" : "") + " and optionally level(lod); "
				"this call passes " + std::to_string(arguments.size()));
		}

		if (arguments[0]->kind != ExpressionKind::Identifier) {
			throw CompileError("the first argument of sample has to name a sampler");
		}
		const Binding* samplerBinding = findResourceBinding(arguments[0]->name);
		if (!samplerBinding || samplerBinding->pointeeMsl.resource != ResourceKind::Sampler) {
			throw CompileError("\"" + arguments[0]->name + "\" is not a sampler, and sample takes one "
				"as its first argument");
		}
		const Binding& sampler = *samplerBinding;

		if (kind != ResourceKind::Texture2D && sampler.unnormalizedSampler) {
			throw CompileError(std::string("a ") + resourceKindName(kind) + " cannot be sampled with a "
				"coord::pixel sampler: Vulkan allows unnormalized coordinates on non-arrayed 1D and 2D "
				"image views only. A sampler parameter is not checked, since its state is the host's");
		}
		const uint32_t width = textureCoordinateWidth(kind);
		const Id coordinateVector = _types.vector(ScalarKind::Float, width);
		Id coordinate = emitExpression(*arguments[1]);
		const Id coordinateType = _builder.typeOf(coordinate);
		if (coordinateType != coordinateVector) {
			if (_types.vectorWidth(coordinateType) != 1 || _types.bitWidth(coordinateType) < 8) {
				throw CompileError("the coordinate of sample has to be a float" + std::to_string(width)
					+ " or a number");
			}
			coordinate = broadcast(coordinate, coordinateVector);
		}
		if (arrayed) {
			coordinate = appendArrayLayer(coordinate, ScalarKind::Float, *arguments[2], "sample");
		}

		Id lod = InvalidId;
		if (arguments.size() == fixed + 1) {
			const Expression& option = *arguments[fixed];
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
				throw CompileError(std::string("the ") + (arrayed ? "fourth" : "third")
					+ " argument of sample has to be level(lod); an offset and the other options "
					"are not lowered yet");
			}
		}

		// Vulkan requires Lod 0 through an unnormalized sampler, whatever level() says.
		if (sampler.unnormalizedSampler) {
			lod = _builder.emitDeclTyped(spirv::OpConstant, _types.scalar(ScalarKind::Float), { 0u });
		}

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id loadedSampler = loadFrom(sampler.id, sampler.pointeeType);
		const Id combined = _builder.emitTyped(spirv::OpSampledImage, _types.sampledImage(texture.pointeeMsl.resource),
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

	// read(uint2 coordinate [, lod]), a uint3 coordinate on a texture3d, or a uint2
	// coordinate and a uint layer on a texture2d_array: a texel by integer
	// coordinate, with no sampler. Metal takes uint and ushort vectors and rejects
	// int and float ones.
	Id Emitter::emitTextureRead(const Expression& call, const Binding& texture) {
		const auto& arguments = call.arguments;
		const bool arrayed = texture.pointeeMsl.resource == ResourceKind::Texture2DArray;
		const size_t fixed = arrayed ? 2 : 1;
		if (arguments.size() < fixed || arguments.size() > fixed + 1) {
			throw CompileError(std::string("read takes a coordinate")
				+ (arrayed ? ", an array index" : "") + " and optionally a lod; this call passes "
				+ std::to_string(arguments.size()));
		}

		const uint32_t width = textureCoordinateWidth(texture.pointeeMsl.resource);
		Id coordinate = emitExpression(*arguments[0]);
		const Id coordinateType = _builder.typeOf(coordinate);
		const bool unsignedVector = _types.vectorWidth(coordinateType) == width
			&& !_types.isFloat(coordinateType) && !_types.isSignedInt(coordinateType)
			&& (_types.bitWidth(coordinateType) == 32 || _types.bitWidth(coordinateType) == 16);
		if (!unsignedVector) {
			throw CompileError("the coordinate of read has to be a uint" + std::to_string(width)
				+ " or a ushort" + std::to_string(width));
		}
		coordinate = convert(coordinate, coordinateType, _types.vector(ScalarKind::UInt, width));
		if (arrayed) {
			coordinate = appendArrayLayer(coordinate, ScalarKind::UInt, *arguments[1], "read");
		}

		Id lod = arguments.size() == fixed + 1 ? emitLod(*arguments[fixed], "read") : constantU32(0);

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id fetched = _builder.emitTyped(spirv::OpImageFetch,
			_types.vector(ScalarKind::Float, 4), { image, coordinate, kImageOperandsLod, lod });
		return texturePixels(texture, fetched);
	}

	// get_width(), get_height() and get_depth() or get_array_size() (component 0,
	// 1 and 2 of the size), of level 0 or of the lod given.
	Id Emitter::emitTextureSize(const Expression& call, const Binding& texture, uint32_t component) {
		const std::string& name = call.left->memberName;
		if (call.arguments.size() > 1) {
			throw CompileError(name + " takes an optional lod; this call passes "
				+ std::to_string(call.arguments.size()));
		}

		const bool storage = texture.pointeeMsl.textureAccess == TextureAccess::Write;
		const Id lod = storage ? InvalidId
			: call.arguments.empty() ? constantU32(0) : emitLod(*call.arguments[0], name);

		if (!_imageQueryDeclared) {
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageQuery) });
			_imageQueryDeclared = true;
		}

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id size = _builder.emitTyped(storage ? spirv::OpImageQuerySize : spirv::OpImageQuerySizeLod,
			_types.vector(ScalarKind::UInt, textureSizeWidth(texture.pointeeMsl.resource)),
			storage ? std::vector<uint32_t>{ image } : std::vector<uint32_t>{ image, lod });
		return _builder.emitTyped(spirv::OpCompositeExtract, _uintType, { size, component });
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

	// An interpolation attribute as SPIR-V decorations. center_perspective is
	// Vulkan's default and decorates nothing; the centroid and sample forms add
	// Centroid or Sample, and the no_perspective forms add NoPerspective. Sample
	// needs the SampleRateShading capability.
	void Emitter::decorateInterpolation(Id variable, Interpolation interpolation) {
		const auto decorate = [&](spirv::DecorationValue decoration) {
			_builder.emit(spirv::OpDecorate, { variable, static_cast<uint32_t>(decoration) });
		};

		switch (interpolation) {
			case Interpolation::None:
			case Interpolation::CenterPerspective:
				return;
			case Interpolation::Flat:
				decorate(spirv::Decoration::Flat);
				return;
			case Interpolation::CenterNoPerspective:
				decorate(spirv::Decoration::NoPerspective);
				return;
			case Interpolation::CentroidPerspective:
				decorate(spirv::Decoration::Centroid);
				return;
			case Interpolation::CentroidNoPerspective:
				decorate(spirv::Decoration::Centroid);
				decorate(spirv::Decoration::NoPerspective);
				return;
			case Interpolation::SamplePerspective:
			case Interpolation::SampleNoPerspective:
				if (!_sampleRateShadingDeclared) {
					_builder.emit(spirv::OpCapability,
						{ static_cast<uint32_t>(spirv::Capability::SampleRateShading) });
					_sampleRateShadingDeclared = true;
				}
				decorate(spirv::Decoration::Sample);
				if (interpolation == Interpolation::SampleNoPerspective) {
					decorate(spirv::Decoration::NoPerspective);
				}
				return;
		}
	}

	// The "locnN" a [[user(...)]] name spells, N being the Location it gives. Only
	// the canonical decimal spelling counts, so locn01 is a plain name, as it is a
	// different name from locn1 on Apple; two fields cannot then claim one Location
	// without repeating a name.
	static std::optional<uint32_t> locnLocation(const std::string& name) {
		constexpr size_t prefix = 4;
		constexpr size_t maxDigits = 9;
		if (name.compare(0, prefix, "locn") != 0 || name.size() == prefix) {
			return std::nullopt;
		}
		const std::string digits = name.substr(prefix);
		if (digits.find_first_not_of("0123456789") != std::string::npos
			|| (digits.size() > 1 && digits[0] == '0')) {
			return std::nullopt;
		}
		if (digits.size() > maxDigits) {
			throw CompileError("[[user(" + name + ")]] names a Location too large to be one");
		}
		return static_cast<uint32_t>(std::stoul(digits));
	}

	// The Location of each field of a struct that crosses from the vertex to the
	// fragment stage, nullopt for [[position]]. A [[user(locnN)]] field is at N;
	// every other field takes the first Location no locnN field of the struct
	// claims, in declaration order. A module sees only its own struct, so a
	// Location has to follow from the struct alone: a name that is not locnN says
	// nothing about where its partner stands in the other stage.
	static std::vector<std::optional<uint32_t>> stageLocations(const StructDecl& decl) {
		std::set<uint32_t> claimed;
		std::set<std::string> names;
		for (const StructField& field: decl.fields) {
			if (!field.attributes.userName) {
				continue;
			}
			if (!names.insert(*field.attributes.userName).second) {
				throw CompileError("duplicated user-defined name \"" + *field.attributes.userName
					+ "\" in struct \"" + decl.name + "\"");
			}
			const auto location = field.attributes.position
				? std::nullopt : locnLocation(*field.attributes.userName);
			if (location) {
				claimed.insert(*location);
			}
		}

		std::vector<std::optional<uint32_t>> locations;
		uint32_t next = 0;
		for (const StructField& field: decl.fields) {
			if (field.attributes.position) {
				locations.emplace_back(std::nullopt);
			} else if (const auto named = field.attributes.userName
				? locnLocation(*field.attributes.userName) : std::nullopt) {
				locations.emplace_back(*named);
			} else {
				while (claimed.count(next) != 0) {
					++next;
				}
				locations.emplace_back(next++);
			}
		}
		return locations;
	}

	// One variable per field. The [[position]] field is a builtin, Position on a
	// vertex output and FragCoord on a fragment input; every other field takes the
	// next Location in declaration order (stageLocations), which is how Iridium numbers them on
	// both sides (indium src/iridium/air.cpp: the struct return loop and the
	// air.fragment_input case), so the two stages agree when they share a struct.
	std::vector<StageVariable> Emitter::declareStageStruct(const StructDecl& decl,
		spirv::StorageClassValue storageClass) {

		const bool isInput = storageClass == spirv::StorageClass::Input;
		std::vector<StageVariable> variables;
		const std::vector<std::optional<uint32_t>> locations = stageLocations(decl);
		bool hasPosition = false;

		for (size_t index = 0; index < decl.fields.size(); ++index) {
			const StructField& field = decl.fields[index];
			const std::string what = "field \"" + field.name + "\" of \"" + decl.name + "\"";

			if (field.attributes.depthMode) {
				throw CompileError(what + " has [[depth]], which is only valid on a fragment output");
			}
			if (field.attributes.sampleMask) {
				throw CompileError(isInput
					? what + " has [[sample_mask]] on a fragment input, which mslc does not lower yet"
					: what + " has [[sample_mask]], which is not valid on a vertex output");
			}
			if (field.attributes.attributeIndex) {
				throw CompileError(what + " has [[attribute(n)]], which mslc does not lower "
					"on a struct crossing from the vertex to the fragment stage");
			}

			if (field.attributes.colorIndex) {
				throw CompileError(isInput
					? what + " is [[color(n)]] on a fragment input, which is framebuffer fetch: mslc does "
						"not lower it, because Vulkan reads a previous colour attachment only through a "
						"subpass input attachment, which needs a descriptor mslc does not assign"
					: what + " is [[color(n)]], which is not valid on a vertex output; it names a "
						"fragment output's colour attachment");
			}
			const Interpolation interpolation = field.attributes.interpolation;
			if (field.attributes.position && interpolation != Interpolation::None) {
				throw CompileError(what + " is [[position]], which cannot also have an interpolation attribute");
			}

			const StageVariable variable = declareStageVariable(field.type, storageClass, what);
			const bool integral = !_types.isFloat(variable.interfaceType);

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
					static_cast<uint32_t>(spirv::Decoration::Location), *locations[index] });

				if (integral && interpolation != Interpolation::None && interpolation != Interpolation::Flat) {
					throw CompileError(what + " is " + typeName(field.type) + ", which requires [[flat]]; "
						"an integer is not interpolated, and Apple's compiler rejects any other qualifier");
				}

				decorateInterpolation(variable.variable, interpolation);

				// VUID-StandaloneSpirv-Flat-04744: an integer fragment input is not
				// interpolated, and the module is rejected unless it says so.
				if (isInput && integral && interpolation == Interpolation::None) {
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

	// A vertex function's [[stage_in]] struct: one Input per field, at the Location
	// the field's [[attribute(n)]] names. Iridium decorates an air.vertex_input
	// parameter with its air.location_index (indium src/iridium/air.cpp:652-660)
	// and indium builds the pipeline's VkVertexInputAttributeDescription with
	// location = the vertex descriptor's attribute index
	// (src/indium/render-pipeline.cpp:114), so the Location is n, not the field's
	// position in the struct. The reflection lists each so a consumer can see
	// which attributes the function reads.
	std::vector<StageVariable> Emitter::declareVertexAttributes(const StructDecl& decl) {
		std::vector<StageVariable> variables;
		std::set<uint32_t> used;

		if (decl.fields.empty()) {
			throw CompileError("[[stage_in]] struct \"" + decl.name + "\" of vertex function \""
				+ _entryPoint->name + "\" has no fields");
		}

		for (const StructField& field: decl.fields) {
			const std::string what = "field \"" + field.name + "\" of \"" + decl.name + "\"";
			const Type& type = field.type;

			if (field.attributes.depthMode) {
				throw CompileError(what + " has [[depth]], which is only valid on a fragment output");
			}
			if (field.attributes.sampleMask) {
				throw CompileError(what + " has [[sample_mask]], which is not valid on a vertex input");
			}
			if (field.attributes.position) {
				throw CompileError(what + " is [[position]], which a vertex function's [[stage_in]] "
					"struct cannot carry");
			}
			if (field.attributes.interpolation != Interpolation::None || field.attributes.colorIndex) {
				throw CompileError(what + " has an interpolation or colour attribute, which is not "
					"valid on a vertex attribute; an interpolation attribute belongs on a field "
					"that crosses to the fragment stage");
			}
			if (!field.attributes.attributeIndex) {
				throw CompileError(what + " has no [[attribute(n)]]; every field of a vertex "
					"function's [[stage_in]] struct needs one");
			}
			const uint32_t index = *field.attributes.attributeIndex;
			if (!used.insert(index).second) {
				throw CompileError(what + " reuses [[attribute(" + std::to_string(index) + ")]]");
			}

			if (type.isPointer || !type.namedType.empty() || type.isMatrix() || type.isPacked) {
				throw CompileError(what + " is a " + typeName(type) + ", which Apple's compiler does "
					"not allow as a vertex attribute; use a scalar or a vector");
			}
			if (type.scalar != ScalarKind::Half && scalarBitWidth(type.scalar) != 32
				&& type.scalar != ScalarKind::Bool) {
				throw CompileError(what + " is a " + typeName(type) + ", which mslc does not lower as "
					"a vertex attribute yet; use a 32-bit scalar or vector, or half");
			}
			if (type.scalar == ScalarKind::Bool) {
				throw CompileError(what + " is a " + typeName(type) + ", which mslc does not lower as "
					"a vertex attribute; use an int or a uint");
			}

			variables.push_back(declareStageVariable(type, spirv::StorageClass::Input,
				"vertex attribute " + what));
			_builder.emit(spirv::OpDecorate, { variables.back().variable,
				static_cast<uint32_t>(spirv::Decoration::Location), index });
			addReflectionEntry("{ \"kind\": \"VertexInput\", \"metal_index\": "
				+ std::to_string(index) + ", \"location\": " + std::to_string(index)
				+ ", \"name\": \"" + field.name + "\" }");
		}

		return variables;
	}

	std::vector<StageVariable> Emitter::declareFragmentOutputs(const StructDecl& decl) {
		constexpr uint32_t maxColorAttachments = 8;
		std::vector<StageVariable> variables;
		std::set<uint32_t> used;
		bool hasDepth = false;
		bool hasSampleMask = false;

		for (const StructField& field: decl.fields) {
			const std::string what = "field \"" + field.name + "\" of \"" + decl.name + "\"";

			if (field.attributes.depthMode) {
				if (field.attributes.colorIndex || field.attributes.position || field.attributes.attributeIndex
					|| field.attributes.sampleMask) {
					throw CompileError(what + " has an incompatible depth attribute combination");
				}
				if (field.attributes.userName || field.attributes.interpolation != Interpolation::None) {
					throw CompileError(what + " combines depth with user or interpolation, which mslc does not lower");
				}
				if (hasDepth) {
					throw CompileError("struct \"" + decl.name + "\" has more than one [[depth]] field");
				}
				const Type& type = field.type;
				if (type.scalar != ScalarKind::Float || !type.isScalar() || type.isPointer
					|| type.arrayLength || !type.namedType.empty()) {
					throw CompileError(what + " has [[depth]], which has to be a scalar float");
				}
				hasDepth = true;
				variables.push_back(declareStageVariable(type, spirv::StorageClass::Output, what));
				_builder.emit(spirv::OpDecorate, { variables.back().variable,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(spirv::BuiltIn::FragDepth) });
				continue;
			}
			if (field.attributes.sampleMask) {
				if (field.attributes.colorIndex || field.attributes.position || field.attributes.attributeIndex) {
					throw CompileError(what + " has an incompatible sample_mask attribute combination");
				}
				if (field.attributes.userName || field.attributes.interpolation != Interpolation::None) {
					throw CompileError(what + " combines sample_mask with user or interpolation, which mslc does not lower");
				}
				if (hasSampleMask) {
					throw CompileError("struct \"" + decl.name + "\" has more than one [[sample_mask]] field");
				}
				const Type& type = field.type;
				if (type.scalar != ScalarKind::UInt || !type.isScalar() || type.isPointer
					|| type.arrayLength || !type.namedType.empty()) {
					throw CompileError(what + " has [[sample_mask]], which has to be a scalar uint");
				}
				hasSampleMask = true;
				const Id array = _types.packedStorage(ScalarKind::UInt, 1);
				const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
					_types.pointer(spirv::StorageClass::Output, array),
					{ static_cast<uint32_t>(spirv::StorageClass::Output) });
				_interface.push_back(variable);
				variables.push_back({ variable, array, _types.scalar(ScalarKind::UInt) });
				_builder.emit(spirv::OpDecorate, { variable,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(spirv::BuiltIn::SampleMask) });
				continue;
			}
			if (field.attributes.interpolation != Interpolation::None) {
				throw CompileError(what + " has an interpolation attribute, which is not valid on a "
					"fragment output");
			}
			if (field.attributes.position || field.attributes.attributeIndex) {
				throw CompileError(what + " has [[position]] or [[attribute(n)]], which is not valid on "
					"a fragment output");
			}
			if (!field.attributes.colorIndex) {
				throw CompileError(what + " has no [[color(n)]], [[depth(mode)]] or [[sample_mask]]; every field of a "
					"fragment function's returned struct needs one");
			}

			const uint32_t index = *field.attributes.colorIndex;
			if (index >= maxColorAttachments) {
				throw CompileError(what + " is [[color(" + std::to_string(index) + ")]], and Apple's "
					"compiler allows indices 0 to 7");
			}
			if (!used.insert(index).second) {
				throw CompileError(what + " reuses [[color(" + std::to_string(index) + ")]]");
			}

			variables.push_back(declareStageVariable(field.type, spirv::StorageClass::Output, what));
			_builder.emit(spirv::OpDecorate, { variables.back().variable,
				static_cast<uint32_t>(spirv::Decoration::Location), index });
		}

		return variables;
	}

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
				// A bare float4 return is the vertex position (MSL 5.2.3).
				const std::string what = "the value vertex function \"" + _entryPoint->name + "\" returns";
				if (type.isPointer || type.isMatrix() || type.vectorWidth != 4 || type.scalar != ScalarKind::Float) {
					throw CompileError("vertex function \"" + _entryPoint->name + "\" returns " + spelled
						+ ", and a vertex function returns a struct with a [[position]] field or a float4 "
						"position");
				}
				_outputs.push_back(declareStageVariable(type, spirv::StorageClass::Output, what));
				_builder.emit(spirv::OpDecorate, { _outputs.back().variable,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(spirv::BuiltIn::Position) });
				return;
			}

			_outputs.push_back(declareStageVariable(type, spirv::StorageClass::Output,
				"the value fragment function \"" + _entryPoint->name + "\" returns"));
			_builder.emit(spirv::OpDecorate, { _outputs.back().variable,
				static_cast<uint32_t>(spirv::Decoration::Location), 0u });
			return;
		}

		const StructDecl* decl = structValue(type);
		if (_entryPoint->stage == Stage::Fragment) {
			if (!decl) {
				throw CompileError("fragment function \"" + _entryPoint->name + "\" returns " + spelled
					+ ", and mslc lowers a returned struct this source declares and nothing else");
			}
			_outputs = declareFragmentOutputs(*decl);
			return;
		}

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
		if (_helper) {
			emitHelperReturn(statement);
			return;
		}

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
			// The fields are extracted one by one, so a buffer's form of the struct
			// serves as it is unless a packed member has to change shape.
			Id copy = value;
			TypeTable::StructForm have;
			TypeTable::StructForm want;
			const bool sameMembers = _types.structForm(_builder.typeOf(value), have)
				&& _types.structForm(declared, want)
				&& have.name == want.name && have.declared == want.declared;
			if (!sameMembers) {
				copy = convertImplicit(value, declared);
			}

			for (size_t i = 0; i < _outputs.size(); ++i) {
				const StageVariable& output = _outputs[i];
				const Id field = _builder.emitTyped(spirv::OpCompositeExtract, output.valueType,
					{ copy, static_cast<uint32_t>(i) });
				const Id stored = structValue(_entryPoint->returnType)->fields[i].attributes.sampleMask
					? _builder.emitTyped(spirv::OpCompositeConstruct, output.interfaceType, { field })
					: convert(field, output.valueType, output.interfaceType);
				_builder.emit(spirv::OpStore, { output.variable, stored });
			}
		} else {
			const StageVariable& output = _outputs.front();
			const Id returned = convert(value, _builder.typeOf(value), declared);
			_builder.emit(spirv::OpStore, { output.variable,
				convert(returned, output.valueType, output.interfaceType) });
		}

		terminate(spirv::OpReturn, { });
	}

	// Locations follow stageLocations, where Apple pairs a vertex output with a
	// fragment input by name (the [[user(name)]] name where a field has one) and
	// type. A vertex and a fragment function pair when every field the fragment
	// reads is one the vertex function writes, and such a pair agrees only when
	// each of those fields is at the same Location in both. A fragment in another
	// library cannot be checked.
	void Emitter::checkStageInterfacesAgree() const {
		struct InterfaceField {
			std::string name;
			std::string type;
			uint32_t location;
			bool operator==(const InterfaceField& other) const {
				return name == other.name && type == other.type;
			}
		};

		const auto interfaceFields = [this](const std::string& name) {
			std::vector<InterfaceField> fields;
			if (const StructDecl* decl = _unit.findStruct(name)) {
				const auto locations = stageLocations(*decl);
				for (size_t index = 0; index < decl->fields.size(); ++index) {
					const StructField& field = decl->fields[index];
					if (!field.attributes.position) {
						fields.push_back({ field.attributes.userName.value_or(field.name),
							typeName(field.type), *locations[index] });
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
					const bool agrees = std::all_of(inputs.begin(), inputs.end(), [&](const auto& input) {
						return std::find_if(outputs.begin(), outputs.end(), [&](const auto& output) {
							return output == input && output.location == input.location;
						}) != outputs.end();
					});
					if (pairs && !agrees) {
						throw CompileError("vertex function \"" + vertex->name + "\" returns "
							+ vertex->returnType.namedType + " and fragment function \"" + fragment->name
							+ "\" takes " + typeName(parameter.type) + " as [[stage_in]]; mslc gives "
							"their fields Locations by declaration order, so the fragment's fields, other "
							"than [[position]], have to be the vertex function's first ones, in order "
							"(or be named [[user(locnN)]], which is Location N in both)");
					}
				}
			}
		}
	}


	// A local variable becomes a Function-storage pointer, which the
	// expression path then loads from, so a local and a parameter behave the
	// same way when a name is used.
	// T& r = place; names the place the initialiser denotes, so a use of r loads
	// from it and a store through r writes to it. A local variable is aliased by
	// copying its binding; an element or a member, or a reference parameter, is a
	// place reached by address, which the binding then holds.
	void Emitter::emitReferenceDeclaration(const VariableDeclaration& declaration) {
		const Expression& place = *declaration.initializer;
		if (place.kind != ExpressionKind::Identifier && place.kind != ExpressionKind::Index
			&& place.kind != ExpressionKind::Member) {
			throw CompileError("the reference \"" + declaration.name + "\" has to be bound to a variable, "
				"an element or a member; binding it to a value is not lowered yet");
		}

		if (place.kind == ExpressionKind::Member && !structOf(*place.left)) {
			throw CompileError("the reference \"" + declaration.name + "\" is bound to a component of a "
				"vector, which is not lowered yet");
		}

		const Expression& root = storeRoot(place);
		if (root.kind != ExpressionKind::Identifier) {
			throw CompileError("the reference \"" + declaration.name + "\" is bound to a place mslc "
				"cannot name");
		}
		const auto rootBinding = _bindings.find(root.name);
		const bool rootReadOnly = rootBinding != _bindings.end() && rootBinding->second.readOnly;
		const bool constReference = declaration.type.isConst
			|| declaration.type.addressSpace == AddressSpace::Constant;
		if (rootReadOnly && !constReference) {
			throw CompileError("the reference \"" + declaration.name + "\" is not const but is bound to "
				"constant memory or a const variable");
		}

		if (place.kind == ExpressionKind::Identifier && rootBinding != _bindings.end()
			&& rootBinding->second.bufferPointeeType == InvalidId && rootBinding->second.isPointer
			&& rootBinding->second.storageClass == spirv::StorageClass::Function) {
			Binding alias = rootBinding->second;
			alias.readOnly = alias.readOnly || constReference;
			requireSameReferent(declaration, alias.pointeeType);
			_bindings[declaration.name] = alias;
			return;
		}

		if (place.kind == ExpressionKind::Identifier && rootBinding != _bindings.end()) {
			rejectBoolReference(root.name, rootBinding->second);
		}
		const Id address = emitPlaceAddress(place);
		const Id pointee = valueTypeAt(address);
		const auto storageClass = _types.storageClassOf(_builder.typeOf(address));
		if (pointee == InvalidId || !storageClass) {
			throw CompileError("cannot determine what the reference \"" + declaration.name + "\" names");
		}
		requireSameReferent(declaration, pointee);

		Binding binding;
		binding.id = address;
		binding.isPointer = true;
		binding.pointeeType = pointee;
		binding.storageClass = *storageClass;
		binding.pointeeMsl = declaration.type;
		binding.structType = declaration.type.namedType.empty()
			? nullptr : _unit.findStruct(declaration.type.namedType);
		binding.readOnly = constReference || rootReadOnly;
		binding.isAddress = true;
		_bindings[declaration.name] = binding;
	}

	// A reference binds without a conversion: a different type would be a
	// temporary, which is a value and not the place.
	void Emitter::requireSameReferent(const VariableDeclaration& declaration, Id referent) {
		if (!declaration.type.namedType.empty()) {
			TypeTable::StructForm have;
			if (!_types.structForm(referent, have) || have.name != declaration.type.namedType) {
				throw CompileError("the reference \"" + declaration.name + "\" is not the type of the place "
					"it is bound to");
			}
			return;
		}
		if (referent != declaredTypeOf(declaration.type)) {
			throw CompileError("the reference \"" + declaration.name + "\" is not the type of the place "
				"it is bound to; a conversion would make a temporary");
		}
	}

	// True when the expression reads something only known when the shader runs: a
	// parameter, a local that is not a constant, an element of a buffer, or a
	// store. Used to refuse such an initialiser on a constexpr; a form that is
	// not recognised as runtime is left to the lowering, which folds what it can.
	bool Emitter::readsRuntimeValue(const Expression& expression) const {
		switch (expression.kind) {
			case ExpressionKind::IntLiteral:
			case ExpressionKind::FloatLiteral:
			case ExpressionKind::BoolLiteral:
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
				const auto local = _bindings.find(expression.name);
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

	// A constexpr brace initializer has to be made of constants. This is checked
	// for a declaration that is never reached too, which is invalid all the same.
	void Emitter::validateConstexprInitializer(const VariableDeclaration& declaration) {
		if (!declaration.type.isConstexpr || !declaration.initializer) {
			return;
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
			if (declaration.type.isScalar() && declaration.type.namedType.empty()) {
				_foldDomainError = false;
				try {
					foldInitializer(declaration.type, *declaration.initializer);
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
			} else if (!canFoldExpression(expression, declaration.name)) {
				throw CompileError("a constexpr brace initializer requires a supported constant expression; this form is not lowered yet");
			}
		};
		validate(validate, *declaration.initializer);
	}

	void Emitter::emitVariableDeclaration(const VariableDeclaration& declaration, bool storageOnly) {
		if (declaration.type.resource != ResourceKind::None) {
			declareLocalSampler(declaration);
			return;
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

		if (!storageOnly) {
			validateConstexprInitializer(declaration);
		}

		Id initial = InvalidId;
		if (!storageOnly && declaration.initializer) {
			const Id initializer = declaration.initializer->kind == ExpressionKind::InitList
				? emitInitListValue(declaration.type, *declaration.initializer)
				: emitExpression(*declaration.initializer);
			initial = convertImplicit(initializer, typeId);
		} else if (!storageOnly) {
			initial = _types.zero(typeId);
		}

		const Id pointerType = _types.pointer(spirv::StorageClass::Function, typeId);
		const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });
		if (!storageOnly) {
			_builder.emit(spirv::OpStore, { id, initial });
		}

		bindLocal(declaration.name, id, typeId, declaration.type);
		const Type& declared = declaration.type;
		if (declaration.initializer && declared.isConst && !declared.isConstexpr && !declared.isPointer
			&& !declared.isVector() && !declared.isMatrix() && declared.namedType.empty()
			&& !declared.arrayLength && !isFloatKind(declared.scalar)
			&& !readsRuntimeValue(*declaration.initializer)) {
			_bindings[declaration.name].constantValue = true;
		}
		if (!storageOnly && declaration.initializer && !_types.isAggregate(typeId)
			&& (declaration.type.isConstexpr || (declaration.type.isConst && !isFloatKind(declaration.type.scalar)))) {
			const Expression* expression = declaration.initializer.get();
			while (expression->kind == ExpressionKind::InitList && expression->elements.size() == 1) {
				expression = expression->elements.front().value.get();
			}
			FoldedConstant folded;
			folded.scalar = declaration.type.scalar;
			const bool empty = expression->kind == ExpressionKind::InitList && expression->elements.empty();
			if (empty || canFoldExpression(*expression, declaration.name)) {
				if (!empty) { folded = foldExpression(*expression); }
				const ScalarKind to = declaration.type.scalar;
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
		if (!storageOnly && declaration.type.isConst && declaration.initializer
			&& declaration.type.namedType.empty() && declaration.type.isScalar()
			&& !isFloatKind(declaration.type.scalar)) {
			try {
				_bindings[declaration.name].folded = foldInitializer(declaration.type, *declaration.initializer);
			} catch (const CompileError&) {
				// A const local may have a runtime initializer without being a constant expression.
			}
		}
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
		_currentBlock = label;
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
				&& it->second.bufferPointeeType == InvalidId
				&& (it->second.storageClass == spirv::StorageClass::Function
					|| it->second.storageClass == spirv::StorageClass::PhysicalStorageBuffer)) {
				address = it->second.id;
			} else if (it != _bindings.end() && it->second.bufferPointeeType != InvalidId
				&& !it->second.isBuffer) {
				// A reference parameter is the one value its buffer holds.
				rejectBoolReference(left.name, it->second);
				address = bufferBase(it->second);
			} else {
				address = emitExpression(left);
			}
		} else if (left.kind == ExpressionKind::Conditional) {
			throw CompileError("assigning to a conditional expression is not supported; "
				"assign inside an if instead");
		} else {
			address = emitExpression(left);
		}

		return address;
	}

	// v.xy = e, s.pos.zw = e, buf[i].w = e: the lanes the swizzle names are
	// replaced in the whole vector and the vector is stored back, so the lanes it
	// does not name keep their value. The address is computed once.
	void Emitter::emitSwizzleStore(const Expression& target, const Expression& valueExpression,
		std::optional<BinaryOperator> compound) {
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

		const Expression* root = &storeRoot(base);
		const auto rootBinding = _bindings.find(root->name);
		if (rootBinding != _bindings.end() && rootBinding->second.readOnly) {
			throw CompileError("assigning to " + quoted + " of \"" + root->name
				+ "\", which is const or in constant memory");
		}

		const Id address = emitPlaceAddress(base);
		const Id addressType = _builder.typeOf(address);
		const Id vectorType = valueTypeAt(address);
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
		const Id component = _types.componentOf(vectorType);
		const auto count = static_cast<uint32_t>(lanes.size());

		const Id old = inBuffer ? loadFromBuffer(address, vectorType) : loadFrom(address, vectorType);
		if (compound) {
			// "v.xy += e" is "v.xy = v.xy + e": the lanes the swizzle names are
			// read out of the one load, combined, and written back by the same
			// shuffle a plain store uses.
			const Id lanesType = _types.withWidth(vectorType, count);
			const Id current = count == 1
				? _builder.emitTyped(spirv::OpCompositeExtract, component, { old, lanes[0] })
				: _builder.emitTyped(spirv::OpVectorShuffle, lanesType, [&] {
					std::vector<uint32_t> operands = { old, old };
					operands.insert(operands.end(), lanes.begin(), lanes.end());
					return operands;
				}());
			value = emitCompoundOperation(*compound, current, value);
		}
		const Id valueType = _builder.typeOf(value);
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
		// ++x and x++ as a statement are x += 1; their value is not lowered.
		if (expression.kind == ExpressionKind::Unary && isStep(expression.unaryOperator)) {
			Expression one;
			one.kind = ExpressionKind::IntLiteral;
			one.intValue = 1;
			one.line = expression.line;
			const bool increment = expression.unaryOperator == UnaryOperator::PreIncrement
				|| expression.unaryOperator == UnaryOperator::PostIncrement;
			emitAssignment(*expression.left, one,
				increment ? BinaryOperator::Add : BinaryOperator::Subtract);
			return;
		}

		// A call whose value is dropped, which is the only way to call a void helper.
		if (expression.kind == ExpressionKind::Call) {
			const Expression* receiver = nullptr;
			if (HelperFunction* helper = findHelper(expression, receiver)) {
				emitHelperCall(expression, *helper, receiver);
				return;
			}
			if (expression.left->kind == ExpressionKind::Member && expression.left->memberName == "write") {
				emitTextureWrite(expression, textureReceiver(expression));
				return;
			}
		}

		// An assignment yields an address rather than a value, so it is
		// handled here rather than through emitExpression.
		if (expression.kind != ExpressionKind::Assign) {
			emitExpression(expression);
			return;
		}

		emitAssignment(*expression.left, *expression.right, expression.compoundOperator);
	}

	// The variable a store lands in: the head of a chain of member and element
	// accesses, since constness and storage follow the chain.
	const Expression& Emitter::storeRoot(const Expression& target) const {
		const Expression* root = &target;
		while (root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) {
			root = root->left.get();
		}
		return *root;
	}

	// A store to something that is not a variable of the entry point: a literal,
	// which is what an enumerator is by now, or a file-scope constant.
	void Emitter::requireStorable(const Expression& target) const {
		const Expression& root = storeRoot(target);
		if (root.kind == ExpressionKind::IntLiteral || root.kind == ExpressionKind::FloatLiteral
			|| root.kind == ExpressionKind::BoolLiteral) {
			throw CompileError("cannot assign to a literal or an enumerator constant");
		}
		if (root.kind == ExpressionKind::Identifier && _bindings.find(root.name) == _bindings.end()
			&& _constants.count(root.name)) {
			throw CompileError("cannot store to \"" + root.name + "\", a constant declared at file scope");
		}
	}

	void Emitter::emitAssignment(const Expression& left, const Expression& right,
		std::optional<BinaryOperator> compound) {
		requireStorable(left);

		if (left.kind == ExpressionKind::Member
			&& !structOf(*left.left)) {
			emitSwizzleStore(left, right, compound);
			return;
		}

		const Expression* root = &storeRoot(left);
		if (root->kind == ExpressionKind::Identifier) {
			const auto target = _bindings.find(root->name);
			if (target != _bindings.end() && target->second.readOnly) {
				throw CompileError("cannot store through \"" + root->name + "\", which is in the "
					"constant address space or declared const");
			}
		}

		const Id address = emitPlaceAddress(left);

		// A store's value has the type the address points at, not the type of
		// the address, so the pointee is what the value is converted to.
		const Id addressType = _builder.typeOf(address);
		const Id pointeeType = valueTypeAt(address);
		if (pointeeType == spirv::InvalidId) {
			throw CompileError("cannot determine what \""
				+ left.name + "\" points at, so the store cannot be typed");
		}

		// A store into a buffer carries the Aligned memory operand, which the
		// same layout rule gives the access.
		const auto storageClass = _types.storageClassOf(addressType);

		Id value = InvalidId;
		if (compound) {
			const bool inBuffer = storageClass
				&& *storageClass == spirv::StorageClass::PhysicalStorageBuffer;
			const Id current = inBuffer
				? loadFromBuffer(address, pointeeType) : loadFrom(address, pointeeType);
			value = emitCompoundOperation(*compound, current,
				emitExpression(right));
		} else {
			value = emitExpression(right);
		}

		if (storageClass && *storageClass == spirv::StorageClass::PhysicalStorageBuffer) {
			storeIntoBuffer(address, convertImplicit(value, pointeeType));
			return;
		}

		_builder.emit(spirv::OpStore, { address, convertImplicit(value, pointeeType) });
	}

	// What stands for the value of a constexpr that is never reached: only that it
	// is a constant matters to the declarations after it, not what it holds.
	static FoldedConstant placeholderConstant(const Type& type) {
		FoldedConstant constant;
		constant.scalar = type.scalar;
		return constant;
	}

	// A switch is a selection: OpSelectionMerge names the block after the body,
	// and OpSwitch sends the selector to one case's block, each of which
	// branches to the next for a fall-through or, through a break, to the merge.
	// A switch with no default sends every unmatched value to the merge.
	void Emitter::emitSwitch(const Statement& statement) {
		Id selector = emitExpression(*statement.expression);
		const Id emittedType = _builder.typeOf(selector);

		// The selector arrives as a value, so its MSL type is recovered from the
		// SPIR-V one. A bool is an integer type C++ accepts here; OpSwitch does
		// not, so it switches on the bool's int value, 1 or 0.
		const std::optional<ScalarKind> selectorKind = _types.scalarKindOf(emittedType);
		if (!selectorKind || isFloatKind(*selectorKind)) {
			const auto diagnosticKind = selectorKind ? selectorKind
				: _types.scalarKindOf(_types.componentOf(emittedType));
			std::string name = diagnosticKind ? std::string(scalarKindName(*diagnosticKind)) : "void";
			if (diagnosticKind && _types.vectorWidth(emittedType) > 1) {
				name += std::to_string(_types.vectorWidth(emittedType));
			}
			throw CompileError("statement requires expression of integer type ('" + name + "' invalid)");
		}

		const ScalarKind kind = promotedKind(*selectorKind);
		if (kind != *selectorKind) {
			selector = convert(selector, emittedType, _intType);
		}

		const Id mergeLabel = _builder.nextId();
		Id defaultLabel = InvalidId;
		std::vector<Id> labels(statement.switchCases.size());
		std::vector<uint64_t> literals(statement.switchCases.size(), 0);

		for (Id& label: labels) {
			label = _builder.nextId();
		}

		std::set<uint64_t> seen;
		{
			const BindingScope validationScope(_bindings);
			for (const StatementPtr& child: statement.switchPreamble) {
				if (child->kind == StatementKind::DeclarationStatement) {
					const auto& declaration = *child->declaration;
					if (!statement.switchCases.empty() && (declaration.initializer
						|| declaration.type.resource == ResourceKind::Sampler)) {
						throw CompileError("switch label bypasses variable initialization");
					}
					_bindings[declaration.name] = Binding{};
				}
			}
			for (size_t i = 0; i < statement.switchCases.size(); ++i) {
				const SwitchCase& kase = statement.switchCases[i];
				if (!kase.value) {
					if (defaultLabel != InvalidId) {
						throw CompileError("multiple default labels in one switch");
					}
					defaultLabel = labels[i];
				} else {
					FoldedConstant folded;
					try {
						folded = foldExpression(*kase.value);
					} catch (const CompileError&) {
						throw CompileError("case value is not a constant expression");
					}
					if (folded.isComposite || isFloatKind(folded.scalar)) {
						throw CompileError("case value is not an integer constant expression");
					}
					// A bool case value is its int value, as the selector is.
					if (folded.scalar == ScalarKind::Bool) {
						folded.scalar = ScalarKind::Int;
						folded.integer = folded.boolean ? 1 : 0;
					}

					if (!fitsInKind(folded, kind)) {
						throw CompileError("case value evaluates to " + integerText(folded)
							+ ", which cannot be narrowed to type '" + scalarKindName(kind) + "'");
					}

					literals[i] = normalizeInteger(kind, folded.integer);
					if (!seen.insert(literals[i]).second) {
						FoldedConstant asSelector;
						asSelector.scalar = kind;
						asSelector.integer = literals[i];
						throw CompileError("duplicate case value '" + integerText(asSelector) + "'");
					}
				}
				for (const StatementPtr& child: kase.body) {
					if (child->kind == StatementKind::DeclarationStatement) {
						const auto& declaration = *child->declaration;
						if (i + 1 < statement.switchCases.size() && (declaration.initializer
							|| declaration.type.resource == ResourceKind::Sampler)) {
							throw CompileError("switch label bypasses variable initialization");
						}
						_bindings[declaration.name] = Binding{};
					}
				}
			}
		}
		for (const StatementPtr& child: statement.switchPreamble) {
			if (child->kind == StatementKind::DeclarationStatement) {
				emitVariableDeclaration(*child->declaration, true);
			}
		}
		_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
		std::vector<uint32_t> operands = {
			selector, defaultLabel != InvalidId ? defaultLabel : mergeLabel };
		const uint32_t width = mappingFor(kind).width;
		for (size_t i = 0; i < statement.switchCases.size(); ++i) {
			if (!statement.switchCases[i].value) {
				continue;
			}
			// A literal is as wide as the selector: two words for a 64-bit one.
			operands.push_back(static_cast<uint32_t>(literals[i]));
			if (width > 32) {
				operands.push_back(static_cast<uint32_t>(literals[i] >> 32));
			}
			operands.push_back(labels[i]);
		}
		terminate(spirv::OpSwitch, operands);

		const ControlDepthScope depth(_controlDepth);
		// A break leaves the switch; a continue belongs to the loop around it,
		// which is what a target with no continue label tells the jump lowering.
		_jumpTargets.push_back({ mergeLabel, InvalidId });
		struct PopTarget {
			std::vector<JumpTarget>& targets;
			~PopTarget() { targets.pop_back(); }
		} popTarget{ _jumpTargets };

		for (size_t i = 0; i < statement.switchCases.size(); ++i) {
			beginBlock(labels[i]);
			for (const StatementPtr& child: statement.switchCases[i].body) {
				if (_terminated) {
					if (child->kind == StatementKind::DeclarationStatement) {
						validateConstexprInitializer(*child->declaration);
						emitVariableDeclaration(*child->declaration, true);
						if (child->declaration->type.isConstexpr) {
							_localFolded[_bindings[child->declaration->name].id]
								= placeholderConstant(child->declaration->type);
						}
					}
					continue;
				}
				emitStatement(*child);
			}
			branchUnlessTerminated(i + 1 < labels.size() ? labels[i + 1] : mergeLabel);
		}

		beginBlock(mergeLabel);
	}

	void Emitter::emitStatement(const Statement& statement) {
		switch (statement.kind) {
			case StatementKind::Compound: {
				const BindingScope scope(_bindings);
				// Whatever follows a return in the same block is unreachable,
				// and a block holds nothing after its terminator.
				for (const StatementPtr& child: statement.children) {
					if (_terminated) {
						if (child->kind == StatementKind::DeclarationStatement) {
							validateConstexprInitializer(*child->declaration);
							// Its name still counts as a constant for the declarations after it.
							if (child->declaration->type.isConstexpr) {
								Binding dead;
								dead.id = _builder.nextId();
								_localFolded[dead.id] = placeholderConstant(child->declaration->type);
								_bindings[child->declaration->name] = dead;
							}
						}
						continue;
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
				const Id condition = asCondition(emitExpression(*statement.expression));
				const Id thenLabel = _builder.nextId();
				const Id mergeLabel = _builder.nextId();
				const Id elseLabel = statement.elseBranch ? _builder.nextId() : mergeLabel;

				_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
				terminate(spirv::OpBranchConditional, { condition, thenLabel, elseLabel });

				const ControlDepthScope depth(_controlDepth);
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
				const ControlDepthScope depth(_controlDepth);
				// The condition is outside the loop body, but nothing in it can
				// be a statement, so the targets may be pushed before it.
				_jumpTargets.push_back({ mergeLabel, continueLabel });
				struct PopTarget {
					std::vector<JumpTarget>& targets;
					~PopTarget() { targets.pop_back(); }
				} popTarget{ _jumpTargets };

				// A &&, || or ?: in the condition splits it over several blocks, and
				// OpLoopMerge has to end the header, so it goes in front of them.
				const bool splitsCondition = condition && containsBranchingOperator(*condition);
				Id conditionValue = InvalidId;
				if (condition && !splitsCondition) {
					conditionValue = asCondition(emitExpression(*condition));
				}
				_builder.emit(spirv::OpLoopMerge, { mergeLabel, continueLabel, kLoopControlNone });
				if (splitsCondition) {
					const Id conditionLabel = _builder.nextId();
					terminate(spirv::OpBranch, { conditionLabel });
					beginBlock(conditionLabel);
					conditionValue = asCondition(emitExpression(*condition));
				}
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

			// Both leave the current block for good, so what follows in the same
			// source block is dropped, as after a return. Inside an if the
			// branch leaves the selection for the loop's own blocks, which
			// structured control flow allows.
			case StatementKind::Break: {
				if (_jumpTargets.empty()) {
					throw CompileError("'break' statement not in loop or switch statement");
				}
				terminate(spirv::OpBranch, { _jumpTargets.back().breakLabel });
				return;
			}

			case StatementKind::Continue: {
				for (auto target = _jumpTargets.rbegin(); target != _jumpTargets.rend(); ++target) {
					if (target->continueLabel != InvalidId) {
						terminate(spirv::OpBranch, { target->continueLabel });
						return;
					}
				}
				throw CompileError("'continue' statement not in loop statement");
			}

			// OpKill ends the block for good, so what follows in the same source
			// block is unreachable, as after a return.
			case StatementKind::Discard:
				// A helper is not a stage, and a helper that returns a value cannot
				// drop the fragment that called it without changing the ABI mslc
				// has fixed. Apple accepts the helper; mslc reports it.
				if (_helper) {
					throw CompileError("discard_fragment() in a helper function is not lowered yet; "
						"mslc drops the fragment in a fragment function only");
				}
				if (_entryPoint->stage != Stage::Fragment) {
					throw CompileError(std::string("discard_fragment() is not allowed within a ")
						+ (_entryPoint->stage == Stage::Vertex ? "vertex" : "kernel")
						+ " function: it drops the fragment, and this function has none");
				}
				terminate(spirv::OpKill, {});
				return;
			case StatementKind::Switch: {
				// A local declared under one label is visible under the ones after
				// it, as C++ says, so the whole body shares one scope.
				const BindingScope scope(_bindings);
				emitSwitch(statement);
				return;
			}
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
			if (const StructDecl* decl = structValue(_entryPoint->returnType)) {
				for (const StructField& field: decl->fields) {
					if (!field.attributes.depthMode) {
						continue;
					}
					_builder.emit(spirv::OpExecutionMode, { _entryPointId,
						static_cast<uint32_t>(spirv::ExecutionMode::DepthReplacing) });
					if (*field.attributes.depthMode != DepthMode::Any) {
						_builder.emit(spirv::OpExecutionMode, { _entryPointId,
							static_cast<uint32_t>(*field.attributes.depthMode == DepthMode::Less
								? spirv::ExecutionMode::DepthLess : spirv::ExecutionMode::DepthGreater) });
					}
				}
			}
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

		// The helpers this entry point called, and the ones those called.
		for (size_t i = 0; i < _helperQueue.size(); ++i) {
			emitHelper(*_helperQueue[i]);
		}
		_helperQueue.clear();
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
		_functionTypes[{ _voidType }] = functionType;

		for (const FunctionDecl& helper: _unit.helpers) {
			if (helper.body) {
				_helpers[helper.name].definition = &helper;
			}
		}

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
			_fileScopeSamplerBindings.clear();
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
	validateHelpers(unit);
	TypeTable types(builder, unit);
	Emitter emitter(builder, unit, entryPoints, options, types);
	return emitter.run();
}

}
