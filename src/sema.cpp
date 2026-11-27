#include "sema.h"

#include "lexer.h"
#include "layout.h"
#include "parser.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
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
		// The screen-space partial derivative along x or along y.
		Dfdx,
		Dfdy,
		// Two vectors of one type and a scalar eta.
		Refract,
		// Two three-component vectors.
		Cross,
		// Round to nearest with halves away from zero, which GLSL.std.450's Round
		// leaves to the implementation, so it is built from Trunc, FAbs and FSign.
		RoundHalfAway,
		// -1, 0 or 1 with the zeros keeping their sign and a NaN giving 0, where
		// GLSL.std.450's FSign leaves both open.
		SignExact,
		// fmod: the remainder with the dividend's sign, which is OpFRem and not an
		// extended instruction.
		FloatRemainder,
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
		{ "fmod", 2, MathShape::FloatRemainder, kNoInstruction, kNoInstruction, kNoInstruction },
		{ "round", 1, MathShape::RoundHalfAway, 3, kNoInstruction, kNoInstruction },
		{ "rint", 1, MathShape::Componentwise, 2, kNoInstruction, kNoInstruction },
		{ "sign", 1, MathShape::SignExact, 6, kNoInstruction, kNoInstruction },
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
		// NMin and NMax (79, 80) return the other operand when one is a NaN, as Metal does;
		// FMin and FMax leave that case open.
		{ "min", 2, MathShape::Componentwise, 79, 39, 38 },
		{ "max", 2, MathShape::Componentwise, 80, 42, 41 },
		// NClamp (81) turns a NaN operand into the lower bound, as Metal does, where FClamp is
		// undefined for it; saturate below is a clamp to [0, 1].
		{ "clamp", 3, MathShape::Componentwise, 81, 45, 44 },
		{ "saturate", 1, MathShape::Saturate, 81, kNoInstruction, kNoInstruction },
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
		{ "dfdx", 1, MathShape::Dfdx, kNoInstruction, kNoInstruction, kNoInstruction },
		{ "dfdy", 1, MathShape::Dfdy, kNoInstruction, kNoInstruction, kNoInstruction },
	};

	// A construct whose type is void: the cast (void)e.
	static bool isVoidCast(const Expression& expression) {
		return expression.kind == ExpressionKind::Construct && expression.constructType
			&& expression.constructType->scalar == ScalarKind::Void
			&& !expression.constructType->isPointer && !expression.constructType->arrayLength
			&& expression.constructType->namedType.empty() && !expression.constructType->isVector()
			&& !expression.constructType->isMatrix() && expression.arguments.size() == 1;
	}

	// An expression of type void: a cast to void, a conditional whose arms are both
	// void, or a comma whose last operand is.
	static bool isVoidValued(const Expression& expression) {
		switch (expression.kind) {
			case ExpressionKind::Construct: return isVoidCast(expression);
			case ExpressionKind::Comma: return isVoidValued(*expression.right);
			case ExpressionKind::Conditional:
				return isVoidValued(*expression.arguments.at(0)) && isVoidValued(*expression.arguments.at(1));
			default: return false;
		}
	}

	const MathBuiltin* findMathBuiltin(const std::string& name) {
		for (const MathBuiltin& builtin: kMathBuiltins) {
			if (name == builtin.name) {
				return &builtin;
			}
		}

		return nullptr;
	}

	// A parameter or argument as the standard library's overloads see it: a scalar
	// (width 1) or a vector of one scalar kind.
	struct StdlibOperand {
		ScalarKind scalar;
		uint32_t width;
		bool operator==(const StdlibOperand& other) const { return scalar == other.scalar && width == other.width; }
	};

	std::optional<StdlibOperand> stdlibOperand(const Parameter& parameter) {
		const Type& type = parameter.type;
		if (parameter.isReference || type.addressSpace != AddressSpace::None || type.isPointer || type.arrayLength
			|| type.matrixColumns || type.isPacked || !type.namedType.empty() || type.resource != ResourceKind::None
			|| type.isAtomic || !type.enumName.empty()) {
			return std::nullopt;
		}
		return StdlibOperand{type.scalar, type.vectorWidth ? type.vectorWidth : 1};
	}

	// The standard library declares "name" with exactly these operands. Only overloads
	// mslc knows to exist count, so an unknown form reads as no match.
	bool isStdlibSignature(const std::string& name, const std::vector<std::optional<StdlibOperand>>& operands) {
		if (name == "discard_fragment") {
			return operands.empty();
		}
		const MathBuiltin* builtin = findMathBuiltin(name);
		if (!builtin || operands.size() != builtin->arity
			|| std::any_of(operands.begin(), operands.end(), [](const auto& o) { return !o; })) {
			return false;
		}
		const StdlibOperand first = *operands.front();
		const bool isFloat = first.scalar == ScalarKind::Float || first.scalar == ScalarKind::Half;
		// The library's abs, min, max and clamp take every integer kind, unsigned and long too
		// (abs of an unsigned value is the value, so mslc has no instruction for it).
		const bool kindTaken = isFloat
			|| (isIntegerKind(first.scalar) && builtin->signedInstruction != kNoInstruction);
		const auto same = [&](size_t count) {
			return std::all_of(operands.begin(), operands.begin() + count, [&](const auto& o) { return *o == first; });
		};
		switch (builtin->shape) {
			case MathShape::VectorToScalar:
			case MathShape::Vector:
			case MathShape::Dot:
				return isFloat && first.width >= 2 && first.width <= 4 && same(operands.size());
			case MathShape::Cross:
				return isFloat && first.width == 3 && same(operands.size());
			case MathShape::Refract:
				return isFloat && first.width >= 2 && first.width <= 4 && same(2)
					&& *operands[2] == StdlibOperand{first.scalar, 1};
			default:
				return kindTaken && first.width <= 4 && same(operands.size());
		}
	}

	// The merge and function control masks. Every one of them is None: Pure
	// would promise no memory writes, which an entry point that stores to a
	// buffer breaks, and neither flattening nor unrolling changes what the
	// shader computes. Named from the generated table rather than written as 0.
	// A construct that needs the stage of the entry point that reaches it. A library knows
	// no stage, so such a helper cannot be exported from one.
	class StageError: public CompileError {
	public:
		using CompileError::CompileError;
	};

	constexpr uint32_t kFunctionControlNone = spirv::FunctionControl::None;
	// SPIR-V's Linkage Type operand of a LinkageAttributes decoration.
	constexpr uint32_t kLinkageImport = 1;
	constexpr uint32_t kLinkageExport = 0;
	constexpr uint32_t kSelectionControlNone = spirv::SelectionControl::None;
	constexpr uint32_t kLoopControlNone = spirv::LoopControl::None;
	// The Lod bit of the image operands mask, which an explicit-lod sample and
	// an image fetch both carry.
	constexpr uint32_t kImageOperandsLod = 0x2;
// The Offset image operand, a runtime texel offset (ImageGatherExtended on a gather).
constexpr uint32_t kImageOperandsOffset = 0x10;

	// The first of the three SpecIds that hold the grid size of a kernel that skips the threads outside it.
	// SpecIds 0 to 2 are the workgroup size and indium numbers function constants from 3 up to 131075, so
	// these start beyond that.
	constexpr uint32_t kGridSizeSpecializationID = 0x40000;

	// The SpecIds of a function constant [[function_constant(n)]]: its value at this plus n, and a bool that the
	// host sets when it gives the constant a value at the other plus n. Threadgroup argument lengths take the
	// SpecIds from 3 up to the first of these.
	constexpr uint32_t kFunctionConstantSpecializationID = 0x10000;
	constexpr uint32_t kFunctionConstantDefinedSpecializationID = 0x20000;

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

	// Whether a threadgroup or simdgroup barrier is called anywhere in an expression or statement. The callee
	// is not among the names collectIdentifiers returns, so it is looked for here.
	bool callsBarrier(const Expression& expression) {
		if (expression.kind == ExpressionKind::Call && expression.left
			&& expression.left->kind == ExpressionKind::Identifier) {
			std::string name = expression.left->name;
			if (name.compare(0, 7, "metal::") == 0) {
				name.erase(0, 7);
			}
			if (name == "threadgroup_barrier" || name == "simdgroup_barrier") {
				return true;
			}
		}
		for (const Expression* child: { expression.left.get(), expression.right.get() }) {
			if (child && callsBarrier(*child)) return true;
		}
		for (const ExpressionPtr& argument: expression.arguments) {
			if (callsBarrier(*argument)) return true;
		}
		for (const InitializerElement& element: expression.elements) {
			if (callsBarrier(*element.value)) return true;
		}
		return false;
	}

	bool callsBarrier(const Statement& statement) {
		for (const Expression* expression: { statement.expression.get(), statement.forCondition.get(),
			statement.forIncrement.get(), statement.whileCondition.get() }) {
			if (expression && callsBarrier(*expression)) return true;
		}
		for (const std::optional<VariableDeclaration>* declaration: { &statement.declaration, &statement.forInitializer }) {
			if (*declaration && (*declaration)->initializer && callsBarrier(*(*declaration)->initializer)) return true;
		}
		for (const StatementPtr& child: statement.children) {
			if (callsBarrier(*child)) return true;
		}
		for (const Statement* nested: { statement.thenBranch.get(), statement.elseBranch.get(),
			statement.forBody.get(), statement.whileBody.get() }) {
			if (nested && callsBarrier(*nested)) return true;
		}
		for (const SwitchCase& kase: statement.switchCases) {
			if (kase.value && callsBarrier(*kase.value)) return true;
			for (const StatementPtr& child: kase.body) {
				if (callsBarrier(*child)) return true;
			}
		}
		for (const StatementPtr& child: statement.switchPreamble) {
			if (callsBarrier(*child)) return true;
		}
		return false;
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

	// Whether a texture or sampler of type have can stand where one of type want is wanted: the
	// same kind, and for a texture the same element, depth, multisampling and access.
	bool sameResourceType(const Type& have, const Type& want) {
		return have.resource == want.resource
			&& (!want.isTexture() || (have.scalar == want.scalar && have.isDepth == want.isDepth
				&& have.isMultisampled == want.isMultisampled && have.textureAccess == want.textureAccess));
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

uint32_t TypeTable::alignmentOf(Id type, bool packed) const {
	const uint32_t bits = bitWidth(type);
	if (bits == 0 && storageClassOf(type)) {
		// A pointer held in a buffer, as a struct's pointer member is: 8 bytes.
		return 8;
	}
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
			alignment = std::max(alignment, memberLayout(field.type).alignment);
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

void TypeTable::requireCapability(spirv::CapabilityValue capability) {
	if (_capabilities.insert(static_cast<uint32_t>(capability)).second) {
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(capability) });
	}
}

// The Dim, Arrayed flag and capability a texture kind's image type takes; the
// sampled and the storage image of a kind differ only in the Sampled operand.
Id TypeTable::imageOf(ResourceKind kind, ScalarKind sampledType, uint32_t sampled, bool depth, bool multisampled) {
	const bool cube = kind == ResourceKind::TextureCube || kind == ResourceKind::TextureCubeArray;
	const bool oneD = kind == ResourceKind::Texture1D || kind == ResourceKind::Texture1DArray;
	const bool arrayed = kind == ResourceKind::Texture2DArray || kind == ResourceKind::Texture1DArray
		|| kind == ResourceKind::TextureCubeArray;
	const bool buffer = kind == ResourceKind::TextureBuffer;
	if (oneD) {
		requireCapability(sampled == 2 ? spirv::Capability::Image1D : spirv::Capability::Sampled1D);
	}
	if (buffer) {
		requireCapability(sampled == 2 ? spirv::Capability::ImageBuffer : spirv::Capability::SampledBuffer);
	}
	if (kind == ResourceKind::TextureCubeArray) {
		requireCapability(spirv::Capability::ImageCubeArray);
	}
	if (multisampled && arrayed && sampled == 2) {
		requireCapability(spirv::Capability::ImageMSArray);
	}
	return _builder.emitDecl(spirv::OpTypeImage, { scalar(sampledType),
		cube ? spirv::Dim::Cube : kind == ResourceKind::Texture3D ? spirv::Dim::Dim3D
			: buffer ? spirv::Dim::Buffer : oneD ? spirv::Dim::Dim1D : spirv::Dim::Dim2D,
		depth ? 1u : sampled == 2 ? 0u : 2u, arrayed ? 1u : 0u, multisampled ? 1u : 0u, sampled,
		static_cast<uint32_t>(spirv::ImageFormat::Unknown) });
}

Id TypeTable::image(ResourceKind kind, ScalarKind sampledType, bool depth, bool multisampled) {
	Id& cached = _images[{ kind, sampledType, depth, multisampled }];
	if (cached == InvalidId) {
		cached = imageOf(kind, sampledType, 1u, depth, multisampled);
	}

	return cached;
}

Id TypeTable::storageImage(ResourceKind kind, TextureAccess access, ScalarKind element) {
	if (access != TextureAccess::Write && !_storageReadDeclared) {
		_storageReadDeclared = true;
		_builder.emit(spirv::OpCapability,
			{ static_cast<uint32_t>(spirv::Capability::StorageImageReadWithoutFormat) });
	}
	if (access != TextureAccess::Read && !_storageWriteDeclared) {
		_storageWriteDeclared = true;
		_builder.emit(spirv::OpCapability,
			{ static_cast<uint32_t>(spirv::Capability::StorageImageWriteWithoutFormat) });
	}
	Id& cached = _storageImages[{ kind, element }];
	if (cached == InvalidId) {
		cached = imageOf(kind, element, 2u, false, false);
	}
	return cached;
}

Id TypeTable::subpassImage(ScalarKind element) {
	Id& cached = _subpassImages[element];
	if (cached == InvalidId) {
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::InputAttachment) });
		cached = _builder.emitDecl(spirv::OpTypeImage, { scalar(element),
			spirv::Dim::SubpassData, 0u, 0u, 0u, 2u,
			static_cast<uint32_t>(spirv::ImageFormat::Unknown) });
	}
	return cached;
}

Id TypeTable::samplerType() {
	if (_samplerType == InvalidId) {
		_samplerType = _builder.emitDecl(spirv::OpTypeSampler);
	}

	return _samplerType;
}

Id TypeTable::sampledImage(ResourceKind kind, ScalarKind sampledType, bool depth, bool multisampled) {
	const Id imageType = image(kind, sampledType, depth, multisampled);
	Id& cached = _sampledImages[{ kind, sampledType, depth, multisampled }];
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

	uint64_t offset = 0;
	uint32_t alignment = 1;

	for (const StructField& field: decl->fields) {
		if (!field.type.isPointer && !field.type.namedType.empty()) {
			throw CompileError("struct \"" + decl->name + "\" has a field whose type is itself "
				"a struct, which mslc cannot represent yet (\"" + field.name + "\")");
		}

		if (field.type.arrayLength && (field.type.isPacked || field.type.scalar == ScalarKind::Bool)) {
			throw CompileError("the array member \"" + field.name + "\" of \"" + decl->name + "\" is an array of "
				+ (field.type.isPacked ? "a packed vector" : "bool") + ", which mslc does not lay out yet");
		}
		if (field.type.arrayLength && field.type.isPointer) {
			throw CompileError("the array member \"" + field.name + "\" of \"" + decl->name
				+ "\" is an array of pointers, which mslc does not lower");
		}
		if (field.type.isPointer && field.type.addressSpace != AddressSpace::Device
			&& field.type.addressSpace != AddressSpace::Constant) {
			throw CompileError("a pointer member (\"" + field.name + "\" of \"" + decl->name
				+ "\") has to point at device or constant memory to be lowered");
		}
		if (field.type.isPointer && (!field.type.namedType.empty() || field.type.isAtomic)) {
			throw CompileError("a pointer member (\"" + field.name + "\" of \"" + decl->name
				+ "\") to a struct or to atomics is not lowered yet");
		}

		const uint32_t components = field.type.vectorWidth > 1 ? field.type.vectorWidth : 1;
		const VectorLayout layout = memberLayout(field.type);

		Id valueType = field.type.isMatrix()
			? matrix(field.type.scalar, field.type.matrixColumns, field.type.vectorWidth)
			: components > 1
			? vector(field.type.scalar, field.type.vectorWidth)
			: scalar(field.type.scalar);
		// A device or constant pointer member holds a buffer's GPU address, so it is
		// the pointer a pointer parameter is: to the { T runtime_array[] } of its pointee.
		if (field.type.isPointer) {
			valueType = bufferPointer(blockStructFor(valueType, field.type.isPacked));
		}
		if (field.type.arrayLength) {
			for (auto inner = field.type.innerArrayLengths.rbegin(); inner != field.type.innerArrayLengths.rend(); ++inner) {
				valueType = array(valueType, *inner);
			}
			valueType = array(valueType, *field.type.arrayLength);
		}
		outTypes.push_back(valueType);

		offset = (offset + layout.alignment - 1) / layout.alignment * layout.alignment;
		if (offset + layout.size > UINT32_MAX) {
			throw CompileError("struct \"" + decl->name + "\" is larger than 4 GiB, which a buffer offset cannot hold");
		}
		outOffsets.push_back(static_cast<uint32_t>(offset));
		offset += layout.size;

		// The struct's alignment is the largest among its members, and its size is
		// the total rounded up to that, so an array of it steps by a whole number
		// of alignments.
		alignment = std::max(alignment, layout.alignment);
	}

	offset = (offset + alignment - 1) / alignment * alignment;
	if (offset > UINT32_MAX) {
		throw CompileError("struct \"" + decl->name + "\" is larger than 4 GiB, which a buffer offset cannot hold");
	}
	outSize = static_cast<uint32_t>(offset);
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
		if (field.type.isPointer || (field.type.arrayLength && !field.type.namedType.empty())) {
			throw CompileError(std::string(field.type.isPointer ? "a pointer" : "an array of structs")
				+ " field in value struct \"" + name + "\" is not lowered yet");
		}
		Id fieldType = !field.type.namedType.empty() ? valueStruct(field.type.namedType)
			: field.type.isMatrix() ? matrix(field.type.scalar, field.type.matrixColumns, field.type.vectorWidth)
			: field.type.vectorWidth > 1 ? vector(field.type.scalar, field.type.vectorWidth)
			: scalar(field.type.scalar);
		if (fieldType == InvalidId) {
			throw CompileError("undeclared type \"" + field.type.namedType + "\"");
		}
		if (field.type.arrayLength) {
			for (auto inner = field.type.innerArrayLengths.rbegin(); inner != field.type.innerArrayLengths.rend(); ++inner) {
				fieldType = array(fieldType, *inner);
			}
			fieldType = array(fieldType, *field.type.arrayLength);
		}
		fieldTypes.push_back(fieldType);
	}

	// A value of a struct with no members is not representable to Mesa's SPIR-V
	// front end (a parameter, a call argument, a constructed value), so a value
	// struct keeps one placeholder member no source member names. The buffer
	// forms keep the empty struct, so layout and reflection are unchanged.
	if (fieldTypes.empty()) {
		fieldTypes.push_back(scalar(ScalarKind::UInt));
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
		// Two entry points of one module cannot share a name: Vulkan's validation
		// rule is that entry points are identified by name within an execution
		// model, and a module with two of the same is rejected with "Entry points
		// cannot share the same name and ExecutionMode". This is reachable now
		// that a module may carry several kernels, since a header holding a
		// kernel and included twice declares it twice.
		std::set<std::string> names;
		for (const FunctionDecl* function: found) {
			if (!names.insert(function->name).second) {
				throw CompileError("two entry points are named \"" + function->name
					+ "\"; a module may have several kernels, but entry points are "
					"identified by name within one execution model, so the names must differ");
			}
		}

		// A kernel's workgroup size is OpExecutionModeId LocalSizeId over three
		// spec constants at SpecId 0, 1, 2, and indium writes constantID 0, 1, 2
		// once per pipeline against the module it was handed, so there is one
		// set of ids per module. That is why a module carries one size, and it
		// is also why it can carry several kernels: they all read the same
		// three constants, and [[threads_per_threadgroup]] reads that same size
		// rather than naming one of its own.
		//
		// A second set of spec constants is still not emitted: two decorated
		// constants per SpecId validate and no driver can resolve them.
		return found;
	}

	// A source with no entry point at all is a library with no functions, which the caller handles.
	throw CompileError(std::string("no ") + (requested == Stage::Kernel ? "kernel "
		: requested == Stage::Vertex ? "vertex " : requested == Stage::Fragment ? "fragment " : "")
		+ "entry point in this source");
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
		// A matrix constant keeps its elements, column by column, so a product of
		// two can be worked out; the columns themselves are in parts.
		uint32_t matrixColumns = 0;
		uint32_t matrixRows = 0;
		std::vector<double> elements;

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
		// Nonzero for a compute id: how many leading lanes of the uvec3 the
		// declared type wants (1 scalar, 2 or 3), converted to the declared
		// element type.
		uint32_t leadingLanes = 0;
		// True when the name is a descriptor over a wrapped buffer, so indexing
		// it needs a struct member index before the element index.
		bool isBuffer = false;
		// True for a const local and for a buffer in constant memory or declared
		// const: Metal rejects a store through it.
		bool readOnly = false;
		// True when id is the address of an element or a member rather than a
		// variable, as for a reference local bound to one.
		bool isAddress = false;
		// True when id is already the buffer's own pointer, as for a helper's
		// device or constant pointer parameter, rather than a member of the
		// entry point's binding-0 address block that has to be loaded first.
		bool ownAddress = false;
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

	// "::name" while a local of that name is in scope: for the length of one expression
	// (the root of a member and index chain) the local is out of the map, so the lookup
	// finds the file-scope name, which is the file-scope array of that name when there is
	// one. Operands of the chain that are not the root (an index) are not "::name": they
	// suspend the view, so a name there finds the local again.
	class FileScopeView {
	public:
		using Stack = std::vector<FileScopeView*>;
		// Nothing in force: no name is hidden.
		explicit FileScopeView(Stack& views) : _views(views) {}
		// Hides "name" until this view ends; views of other names stay in force beneath it.
		FileScopeView(std::map<std::string, Binding>& bindings, Stack& views, const std::string& name,
			const Binding* array)
			: _bindings(&bindings), _views(views), _name(name), _hasArray(array != nullptr) {
			_local = bindings.extract(name);
			if (array) {
				bindings.emplace(name, *array);
			}
			_engaged = true;
			views.push_back(this);
		}
		// Puts every hidden local back, until this view ends, while an operand of a chain is
		// evaluated: the operand is ordinary code, whichever "::" chains enclose it.
		FileScopeView(Stack& views, bool suspend) : _views(views) {
			if (!suspend) { return; }
			for (auto view = views.rbegin(); view != views.rend(); ++view) {
				if ((*view)->_hiding) {
					(*view)->release();
					_released.push_back(*view);
				}
			}
		}
		~FileScopeView() {
			if (_engaged) {
				release();
				_views.pop_back();
			}
			for (auto view = _released.rbegin(); view != _released.rend(); ++view) {
				(*view)->acquire();
			}
		}
		FileScopeView(const FileScopeView&) = delete;
		FileScopeView& operator=(const FileScopeView&) = delete;

	private:
		// The nodes are moved, never copied, so a reference into either stays valid across a
		// suspension: an emitter that holds the file-scope array's binding while it evaluates
		// an index still reads that binding afterwards.
		void release() {
			if (!_hiding) { return; }
			if (_hasArray) { _file = _bindings->extract(_name); }
			if (!_local.empty()) { _bindings->insert(std::move(_local)); }
			_hiding = false;
		}
		void acquire() {
			_local = _bindings->extract(_name);
			if (_hasArray) { _bindings->insert(std::move(_file)); }
			_hiding = true;
		}

		std::map<std::string, Binding>* _bindings = nullptr;
		Stack& _views;
		std::vector<FileScopeView*> _released;
		std::map<std::string, Binding>::node_type _local;
		std::map<std::string, Binding>::node_type _file;
		std::string _name;
		bool _hasArray = false;
		bool _engaged = false;
		bool _hiding = true;

	public:
		bool hides(const std::string& name) const { return _engaged && _hiding && _name == name; }
		bool hiding() const { return _engaged && _hiding; }
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
		// Framebuffer fetch: the colour attachment read, instead of an Input variable.
		// variable is the input attachment descriptor once declareAttachments has run.
		std::optional<uint32_t> attachment;
		ScalarKind sampled = ScalarKind::Float;
		std::string name;
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

	// Everything about the helper functions that the source alone decides, checked
	// for all of them whether or not an entry point reaches one: the declarations
	// of a name agree, a call names a function declared above it with the right
	// number of arguments, and no function reaches itself. Apple accepts a
	// recursive call, but SPIR-V for Vulkan forbids one, and a module with it
	// fails validation, so it is refused here with the cycle named.
	constexpr std::string_view stdlibPrefix = "metal::";

	// The names the unit gives two or more signatures. A member function keeps one
	// signature per name, which validateHelpers still enforces.
	std::set<std::string> overloadedNames(const TranslationUnit& unit) {
		std::map<std::string, const FunctionDecl*> seen;
		std::set<std::string> overloaded;
		for (const FunctionDecl& helper: unit.helpers) {
			const size_t tail = helper.name.rfind("::");
			if (tail != std::string::npos && unit.findStruct(helper.name.substr(0, tail))) {
				continue;
			}
			const auto previous = seen.emplace(helper.name, &helper);
			if (!previous.second && !sameParameterTypes(*previous.first->second, helper)) {
				overloaded.insert(helper.name);
			}
		}
		return overloaded;
	}

	// What a function is called inside the compiler: its name, and for an overloaded
	// name also its parameters, so each signature is a function of its own.
	std::string helperKey(const FunctionDecl& helper, const std::set<std::string>& overloaded) {
		return overloaded.count(helper.name) ? helper.name + "(" + parameterSignature(helper) + ")" : helper.name;
	}

	// What it costs an argument to reach a parameter of another type, as Apple ranks it (measured):
	// the C++ order, with a scalar's splat into a vector below every scalar conversion.
	enum ConversionRank : int { kExact, kPromotion, kConversion, kSplat, kNoConversion };

	void validateHelpers(const TranslationUnit& unit, bool allowImports) {
		const std::set<std::string> overloaded = overloadedNames(unit);
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

			// A call to an overloaded name is resolved by its arguments' types, which only
			// the emitter has: the by-name checks below skip it (no key is the bare name).
			const std::string key = helperKey(helper, overloaded);
			const auto previous = first.emplace(key, &helper);
			if (!previous.second) {
				if (!mslc::sameParameterTypes(*previous.first->second, helper)) {
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

			if (helper.body && !defined.emplace(key, &helper).second) {
				throw CompileError("redefinition of \"" + helper.name + "\"");
			}

			// A prototype in namespace metal redeclares a standard library function; a
			// definition with one of its parameter lists redefines it.
			if (helper.body && helper.name.compare(0, stdlibPrefix.size(), stdlibPrefix) == 0) {
				std::vector<std::optional<StdlibOperand>> operands;
				for (const Parameter& parameter: helper.parameters) {
					operands.push_back(stdlibOperand(parameter));
				}
				if (isStdlibSignature(helper.name.substr(stdlibPrefix.size()), operands)) {
					throw CompileError("redefinition of \"" + helper.name + "\": the standard library defines it "
						"with these parameters");
				}
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
				// Several names are one overload set, chosen by the arguments at emission.
				if (!call->left->overloadNames.empty()) {
					continue;
				}
				// A template declared above the caller competes with the helper; one below is no candidate.
				if (std::any_of(unit.templates.begin(), unit.templates.end(), [&](const FunctionTemplate& declaration) {
					return declaration.name == name && declaration.order <= caller.order;
				})) {
					continue;
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
				// A Linkage object may call a function it only declares: the definition
				// comes from another translation unit at link time. A Vulkan executable
				// cannot, since nothing would resolve it.
				if (!defined.count(name) && !allowImports) {
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
		for (const auto& [key, definition]: defined) {
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
		for (const auto& [key, definition]: defined) {
			visit(visit, definition->name);
		}
	}

	// Which component of an image size a get_* call reads. An enum class, not a
	// number or bool, so a call written against another signature fails to compile.
	enum class SizeComponent : uint32_t { Width = 0, Height = 1, Depth = 2, ArraySize = 2 };

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
		// Stands in for the entry point while a library's helpers are emitted: no
		// stage, so a helper that needs one (fwidth, a barrier) is refused.
		FunctionDecl _libraryScope;
		const ModuleOptions& _options;
		TypeTable& _types;

		std::map<std::string, Binding> _bindings;
		// File-scope constants, keyed by name. Separate from _bindings because a
		// constant belongs to the module rather than to the entry point, and an
		// entry point's own names are looked up first.
		std::map<std::string, ConstantBinding> _constants;
		// The file-scope function constants, keyed by name. Their specialization constants are declared when
		// the source first reads them, so a constant nothing reads asks for no capability its type needs.
		struct FunctionConstant {
			const VariableDeclaration* declaration = nullptr;
			Id value = InvalidId;
			Id defined = InvalidId;
		};
		std::map<std::string, FunctionConstant> _functionConstants;
		// "constant bool b = is_function_constant_defined(c);": b, and the c it stands for.
		std::map<std::string, std::string> _functionConstantDefinedAliases;
		// The function constants each function reads in the code emitted for it, and the functions each calls:
		// an entry point's reflection lists the constants of every function it reaches. A helper is emitted once
		// for the entry points that share it, so what it reads is kept with it rather than with an entry point.
		std::map<const FunctionDecl*, std::set<std::string>> _functionConstantReads;
		std::map<const FunctionDecl*, std::set<const FunctionDecl*>> _functionCallees;
		// The file-scope samplers, by name, and the ones the entry point being
		// emitted names, which are looked up after its own parameters and locals.
		std::map<std::string, const VariableDeclaration*> _fileScopeSamplers;
		std::map<std::string, Binding> _fileScopeSamplerBindings;
		// The functions that name a file-scope sampler, or call one that does. Each entry point
		// that reaches one gets its own copy, since the sampler is that entry point's variable.
		std::set<const FunctionDecl*> _samplerDependent;
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
		// The bool or bool vector type of a member that holds one by reference.
		std::map<uint32_t, Id> _bufferBoolTypes;
		// The matrix type of a member that holds one by reference, wrapped in a struct.
		std::map<uint32_t, Id> _bufferMatrixTypes;
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
		std::vector<Id> _entryPointIds;
		bool _floatControlsDeclared = false;
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
		bool _gatherExtendedDeclared = false;
		bool _minLodDeclared = false;
		std::map<int32_t, Id> _intConstants;
		bool _sampleRateShadingDeclared = false;
		// Ids the entry point lists as its interface, in declaration order.
		std::vector<Id> _interface;
		// The Workgroup variable of each threadgroup local in the kernel body, declared
		// before the entry point because SPIR-V 1.5 lists every module-scope variable in it.
		std::map<const VariableDeclaration*, Id> _threadgroupLocals;
		// The next SpecId for a threadgroup argument's element count; 0, 1 and 2 are the workgroup size.
		uint32_t _nextSpecId = 3;

		// What a return statement writes: one variable for a returned scalar or
		// vector, or one per field of a returned struct. Empty for a void function.
		std::vector<StageVariable> _outputs;
		// The [[stage_in]] parameter and one Input per field.
		const Parameter* _stageIn = nullptr;
		// The kernel's [[threads_per_threadgroup]] parameter, and the spec constants
		// that hold the workgroup size it reads.
		const Parameter* _threadsPerThreadgroup = nullptr;
		Id _localSize[3] = { InvalidId, InvalidId, InvalidId };
		// The grid size of a kernel that skips the threads outside it (ModuleOptions::maskGridSize): three
		// spec constants shared by the module, and the Input variable of the thread position in the grid
		// the guard reads, which is the kernel's own [[thread_position_in_grid]] when it has one.
		Id _gridSize[3] = { InvalidId, InvalidId, InvalidId };
		Id _globalInvocationVariable = InvalidId;
		bool _masksGridSize = false;
		std::vector<StageVariable> _stageInputs;
		// Framebuffer fetch: a [[color(n)]] parameter and the value it is read into.
		struct AttachmentParameter {
			const Parameter* parameter;
			StageVariable variable;
		};
		std::vector<AttachmentParameter> _attachmentParameters;
		std::set<uint32_t> _attachmentIndices;
		// At most one BuiltIn FragCoord Input variable per fragment entry point,
		// whether it comes from a stage-struct [[position]] field or a direct
		// [[position]]/[[frag_coord]] parameter.
		bool _hasFragCoordInput = false;

		// A helper's body is emitted once per stage that reaches it, or once per entry point when it
		// names a file-scope sampler (helperScope).
		struct HelperFunction {
			const FunctionDecl* definition = nullptr;
			std::map<std::pair<Stage, const FunctionDecl*>, Id> ids;
			Id returnType = InvalidId;
			std::vector<Id> parameterTypes;
			Id type = InvalidId;
			// For a helper with a thread pointer parameter, the length of the caller's
			// array bound to each parameter (zero for any other): the helper is
			// emitted once per distinct list, so its pointer is to the whole array.
			std::vector<uint32_t> arrayLengths;
			// Where the first declaration of this signature sits (FunctionDecl::order):
			// an overload is a candidate only from there down.
			size_t firstOrder = 0;
		};
		// Keyed by helperKey: the name, or for an overloaded name each signature.
		std::map<std::string, HelperFunction> _helpers;
		std::set<std::string> _overloadedNames;
		std::map<std::string, std::vector<HelperFunction*>> _overloads;
		// Which function calls which, so a cycle through overloads, which the by-name check
		// of validateHelpers cannot see, is refused when the call that closes it is emitted.
		std::map<const FunctionDecl*, std::set<const FunctionDecl*>> _callEdges;
		std::map<std::pair<const FunctionDecl*, std::vector<uint32_t>>, HelperFunction> _pointerSpecialisations;
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
			_options(options), _types(types) {
			for (const FunctionTemplate& declaration: unit.templates) {
				_templatesByName[declaration.name].push_back(&declaration);
				if (!declaration.memberOf.empty()) {
					_memberTemplateNames.insert(declaration.name.substr(declaration.name.rfind("::") + 2));
				}
			}
		}

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
		// The laid-out element type of a device or constant pointer's pointee, and
		// the { T runtime_array[] } struct the pointer is to: what an entry point's
		// own buffer parameter binds and what such a helper parameter takes.
		std::pair<Id, Id> bufferParamTypes(const Type& type);
		void loadThreadsPerThreadgroup();
		void declareGridMaskInput();
		void emitGridMask();
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
		FoldedConstant foldScalarConstruct(const Expression& expression);
		FoldedConstant convertScalarForConstruct(FoldedConstant folded, ScalarKind to);
		std::vector<FoldedConstant> foldVectorConstructor(const Expression& expression);
		FoldedConstant foldConditional(const Expression& expression);
		bool constantLeaves(const Expression& expression, const std::string& excluded);
		bool namesConstexprAggregate(const Expression& expression);
		bool provenByFolding(const Expression& expression, const std::string& excluded);
		FoldedConstant foldComparison(BinaryOperator op, FoldedConstant left, FoldedConstant right);
		FoldedConstant foldInitializer(const Type& type, const Expression& initializer, bool braced = false);
		void foldStructInitializer(FoldedConstant& folded, const StructDecl& decl,
			const Expression& initializer);
		FoldedConstant foldMatrix(const Expression& expression, const Type* expected);
		FoldedConstant foldMatrixList(const Type& type, const Expression& initializer);
		FoldedConstant foldMatrixConstruct(const Expression& expression);
		FoldedConstant foldMatrixProduct(const Expression& expression);
		FoldedConstant matrixConstant(ScalarKind kind, uint32_t columns, uint32_t rows,
			std::vector<double> elements);
		bool isMatrixExpression(const Expression& expression);
		std::vector<double> foldColumnConstructor(const Expression& expression, ScalarKind kind, uint32_t rows);
		double foldMatrixScalar(const Expression& expression, ScalarKind kind, bool braced);
		FoldedConstant foldExpression(const Expression& expression);
		FoldedConstant foldBinary(const Expression& expression);
		FoldedConstant foldUnary(const Expression& expression);
		void declareParameters();
		void declareThreadgroupArgument(const Parameter& parameter, size_t index, uint32_t metalIndex);
		void declareResources(const std::vector<std::pair<size_t, const Parameter*>>& resources);
		Id declareDescriptorVariable(Id pointee, uint32_t binding);
		void reserveLocalSamplers(const Statement& statement, const std::set<std::string>& used);
		void reserveThreadgroupLocals(const Statement& statement);
		void reserveThreadgroupLocal(const VariableDeclaration& declaration);
		void reserveSampler(const VariableDeclaration& declaration);
		// The entry point's own name first, then a file-scope sampler it names.
		// A helper the pre-emission scan did not mark would share one body between entry points, so
		// a file-scope sampler it names is refused: that is argument-dependent lookup, a template
		// instance or any other path the scan does not follow.
		const Binding* findResourceBinding(const std::string& name) const {
			const auto own = _bindings.find(name);
			if (own != _bindings.end()) return &own->second;
			const auto fileScope = _fileScopeSamplerBindings.find(name);
			if (fileScope == _fileScopeSamplerBindings.end()) return nullptr;
			if (_helper && _helper->body && !_samplerDependent.count(_helper)) {
				throw CompileError("\"" + _helper->name + "\" names the file-scope sampler \"" + name
					+ "\" but is reached by a path mslc does not follow (argument-dependent lookup or a "
					"template instance); pass the sampler as a parameter or call it by a qualified name");
			}
			return &fileScope->second;
		}
		Binding embeddedSamplerBinding(const VariableDeclaration& declaration) const;
		void reserveFileScopeSamplers();
		void declareFunctionConstant(const VariableDeclaration& declaration);
		void declareFunctionConstantAlias(const VariableDeclaration& declaration, const std::string& read);
		const std::string* functionConstantNamed(const std::string& name) const;
		Id functionConstantValue(const std::string& name, bool defined);
		Id emitIsFunctionConstantDefined(const Expression& call);
		std::string functionConstantsJson();
		std::vector<const FunctionDecl*> functionsCalledBy(const Expression& call);
		void declareLocalSampler(const VariableDeclaration& declaration);
		void bindResourceAlias(const VariableDeclaration& declaration, const Type* declared);
		void addReflectionEntry(const std::string& entry);
		std::string descriptorJson(uint32_t binding) const;
		const Binding& textureReceiver(const Expression& call);
		void emitTextureWrite(const Expression& call, const Binding& texture);
		Id emitTextureCall(const Expression& call);
		Id emitTextureSample(const Expression& call, const Binding& texture, bool compare = false);
		Id compareValue(const Expression& argument, const std::string& callName);
		Id emitTextureRead(const Expression& call, const Binding& texture);
		Id emitMultisampledCall(const Expression& call, const Binding& texture);
		Id emitTextureGather(const Expression& call, const Binding& texture, bool compare = false);
		const Binding& samplerArgument(const Expression& argument, ResourceKind kind, const std::string& callName);
		Id samplingCoordinate(const Expression& coordinate, ResourceKind kind, const Expression* layer, const std::string& callName);
		Id emitTextureSize(const Expression& call, const Binding& texture, SizeComponent component);
		Id literalOffset(const Expression& expression, uint32_t width);
		Id layerIndex(const Expression& layer, const std::string& callName, const char* what = "array index");
		Id appendLayers(Id coordinate, ResourceKind kind, const std::vector<ExpressionPtr>& arguments, size_t first,
			const std::string& callName);
		Id appendArrayLayer(Id coordinate, ScalarKind component, const Expression& layer,
			const std::string& callName);
		Id texturePixels(const Binding& texture, Id sampled);
		Id textureTexel(const Binding& texture, Id sampled);
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
		StageVariable declareFramebufferFetch(const Type& type, uint32_t index, const std::string& name,
			const std::string& what);
		void declareAttachments();
		Id readAttachment(const StageVariable& attachment);
		void emitReturn(const Statement& statement);
		void checkStageInterfacesAgree() const;
		void emitFunctionBody(const Statement& statement);
		void emitStatement(const Statement& statement);
		void emitSwitch(const Statement& statement);
		void emitExpressionStatement(const Expression& expression);
		void emitAssignment(const Expression& left, const Expression& right,
			std::optional<BinaryOperator> compound);
		Id emitPlaceAddress(const Expression& left);
		const Expression& storeRoot(const Expression& target);
		bool readOnlyRoot(const Expression& root);
		void requireStorable(const Expression& target);
		void emitSwizzleStore(const Expression& target, const Expression& valueExpression,
			std::optional<BinaryOperator> compound);
		void emitVariableDeclaration(const VariableDeclaration& declaration, bool storageOnly = false);
		void emitValueDeclaration(const VariableDeclaration& declaration, const Type& type, Id deduced,
			bool storageOnly);
		void emitAutoDeclaration(const VariableDeclaration& declaration);
		Type deduceAutoType(const Type& written, Id valueType, const std::string& name);
		// storage is where the variable itself lives: Function for an ordinary
		// local, Workgroup for a threadgroup one. It is what the index path walks
		// the binding with, so it has to be the real one.
		void bindLocal(const std::string& name, Id variable, Id type, const Type& msl,
			spirv::StorageClassValue storage = spirv::StorageClass::Function);

		// Whether a binding names a variable this compiler declares on its own,
		// rather than one reached through a descriptor. An array lives in
		// whichever of these its address space selects, and every path that walks
		// one has to agree on the list: an array path that omits Workgroup
		// reports a misleading diagnostic or emits an access chain typed at the
		// wrong storage class, and neither is caught until something downstream
		// rejects it. So this is the single place the list lives.
		bool isLocalStorage(spirv::StorageClassValue storage) const;

		// A threadgroup variable is shared by every invocation of the workgroup,
		// so it exists in a kernel and nowhere else, and it takes no initialiser.
		// A scalar and an array share this, so the two cannot drift apart.
		void checkThreadgroupLocal(const VariableDeclaration& declaration);
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
		Id emitUnaryOperation(UnaryOperator op, Id operand, Id type);
		void emitReferenceDeclaration(const VariableDeclaration& declaration);
		void emitLocalArray(const VariableDeclaration& declaration, const Type& type, bool storageOnly);
		void declareThreadgroupLocal(const VariableDeclaration& declaration, const Type& type, Id typeId);
		Id arrayTypeOf(const Type& type);
		struct ArrayLeaves {
			// The value of one element given its list entry, or its zero fill for none.
			std::function<Id(const Type& element, const Expression* entry)> element;
			std::function<Id(Id type, const std::vector<uint32_t>& parts)> compose;
		};
		Id emitArrayInit(const Type& type, const std::vector<InitializerElement>& entries, size_t& position,
			bool braced, const ArrayLeaves& leaves);
		Id emitArrayListValue(const Type& type, const Expression& list);
		Id emitLocalArrayIndex(const Expression& expression, bool asAddress);
		const Binding* arrayRootBinding(const Expression& expression, size_t& depth) const;
		void declareFileScopeArray(const VariableDeclaration& declaration, size_t position);
		void bindFileScopeArrays(size_t declaredBefore);
		std::unique_ptr<FileScopeView> viewFileScope(const Expression& expression);
		FileScopeView::Stack _fileScopeViews;
		struct FileScopeArray {
			std::string name;
			size_t position = 0;
			Id variable = InvalidId;
			Id type = InvalidId;
			Type msl;
		};
		std::vector<FileScopeArray> _fileScopeArrays;
		// The file-scope arrays the function being emitted sees, as bound before any local.
		std::map<std::string, Binding> _fileScopeBound;
		void validateConstexprInitializer(const VariableDeclaration& declaration);
		void validateConstexprInitializer(const VariableDeclaration& declaration, const Type& type);
		std::optional<std::string> enumOf(const Expression& expression);
		void requireEnumValue(const std::string& target, const Expression& value, const std::string& what);
		bool readsRuntimeValue(const Expression& expression) const;
		void requireSameReferent(const VariableDeclaration& declaration, const Type& type, Id referent);
		Id emitIndex(const Expression& expression, bool asAddress);
		bool isElementAccess(const Expression& expression) const;
		Id emitElementAccess(const Expression& expression, bool asAddress);
		struct ElementIndex {
			std::optional<uint32_t> constant;
			Id id = InvalidId;
		};
		ElementIndex elementIndex(const Expression& expression, uint32_t width);
		Id emitMatrixValueColumn(Id matrixValue, const TypeTable::MatrixInfo& matrix, const Expression& index);
		Id wordIndex(Id index, const char* what);
		Id stepIndex(const Expression& expression, const char* what);
		Id emitMember(const Expression& expression);
		const Type* pointerMemberType(const Expression& expression);
		std::optional<Binding> pointerMemberBinding(const Expression& expression);
		size_t arrayMemberRank(const Expression& expression, size_t& outIndices);
		std::optional<Id> emitArrayMemberElement(const Expression& expression, bool asAddress);
		Id emitMemberAddress(const Expression& expression, Id& outFieldType);

		// The file-scope constant an expression names, or null when it names
		// something else. A constant is a value and not a place, which is what
		// tells a member read on one apart from a member read on a local.
		const ConstantBinding* constantFor(const Expression* expression);
		Id emitCall(const Expression& expression);
		HelperFunction* findHelper(const Expression& call, const Expression*& receiver, std::vector<Id>& prepared);
		Id emitHelperCall(const Expression& call, HelperFunction& helper, const Expression* receiver,
			const std::vector<Id>& prepared);
		std::vector<HelperFunction*> helpersNamed(const std::string& name);
		std::vector<std::string> namesOfCall(const Expression& call, std::set<std::string>* adlOnly = nullptr);
		const StructDecl* adlStructOf(const Expression& argument);
		bool inConstantSpace(const Expression& place);
		std::optional<Type> placeType(const Expression& place);
		std::optional<Type> sizeofPlaceType(const Expression& place);
		std::optional<Type> sizeofExpressionType(const Expression& expression);
		bool namesPointerParameter(const std::string& name) const;
		uint64_t sizeofOperand(const Expression& operand);
		bool isVectorPlace(const Expression& place);
		bool isVectorComponent(const Expression& place);
		bool bindsTemporary(const Expression& place);
		std::optional<Type> computedPartType(const Expression& part);
		Id constantReferenceArgument(const Expression& argument, const Parameter& parameter,
			const std::string& function, size_t position, Id prepared);

		// A template's signature, read with the arguments deduced from a call's, is an instance: a helper
		// that competes by the usual ranks, and has its body read only if it is chosen.
		static constexpr size_t kMaxInstantiationDepth = 64;
		struct TemplateInstance {
			const FunctionTemplate* declaration = nullptr;
			std::vector<TemplateArgument> arguments;
			FunctionDecl decl;
			HelperFunction helper;
			size_t depth = 0;
			bool committed = false;
		};
		std::map<std::string, std::vector<const FunctionTemplate*>> _templatesByName;
		std::map<std::string, std::unique_ptr<TemplateInstance>> _instances;
		std::map<const FunctionDecl*, TemplateInstance*> _instanceOf;
		std::map<std::string, std::unique_ptr<FunctionDecl>> _patterns;
		std::vector<const FunctionTemplate*> templatesNamed(const std::string& name) const;
		std::vector<const FunctionTemplate*> visibleTemplates(const std::vector<std::string>& names,
			const std::set<std::string>& adl) const;
		std::vector<const FunctionTemplate*> templatesCalled(const Expression& call) const;
		const FunctionDecl* templatePattern(const FunctionTemplate& declaration, const std::vector<TemplateArgument>& given,
			std::vector<TemplateArgument>& arguments);
		struct OverloadArgument;
		bool deduceTemplateArguments(const FunctionTemplate& declaration, const FunctionDecl& pattern,
			size_t given, const std::vector<OverloadArgument>& arguments, std::vector<TemplateArgument>& deduced);
		TemplateInstance& templateInstance(const FunctionTemplate& declaration, std::vector<TemplateArgument> arguments);
		void commitInstance(TemplateInstance& instance);
		bool isInstance(const FunctionDecl* function) const { return function && _instanceOf.count(function) != 0; }
		bool callsDependentTemplate(const Expression& call);
		bool dependsOnTemplate(const Expression& call);
		std::string describeTemplate(const FunctionTemplate& declaration, const FunctionDecl& pattern) const;
		HelperFunction* definedHelper(HelperFunction* entry);
		// What a call can denote before its arguments are known: every function of every name the
		// parser found for it (not the ones argument-dependent lookup adds).
		std::vector<HelperFunction*> helpersCalled(const Expression& call);
		// A member call's receiver is the argument of the first parameter, "this", and is already evaluated.
		HelperFunction* resolveOverload(const Expression& call, const std::vector<HelperFunction*>& set,
			const std::vector<const FunctionTemplate*>& templates, const std::set<std::string>& adl,
			std::vector<Id>& prepared, const Expression* receiver = nullptr);
		// The member function templates of a struct that a call through "." names, or none.
		std::vector<const FunctionTemplate*> memberTemplatesOf(const std::string& owner, const std::string& member) const;
		std::set<std::string> _memberTemplateNames;
		const Type* returningType(const Expression& call, std::string (*aspect)(const Type&));
		size_t callerOrder() const { return (_helper ? _helper : _entryPoint)->order; }
		void noteCall(const FunctionDecl& callee);
		bool isPlainVariable(const Expression& expression) const;
		bool isWholeVariable(const Expression& expression) const;
		struct PointerFit {
			Type element;
			Type wanted;
			std::vector<uint32_t> shape;
			std::vector<uint32_t> wantedShape;
			bool matches = false;
		};
		PointerFit pointerFit(const Binding& array, const Type& parameter);
		// What a call's argument is, as far as choosing an overload cares: a value, a variable
		// (a place a reference can bind), a local array (what a pointer parameter takes), or a
		// texture or sampler. Only the first two are evaluated.
		struct OverloadArgument {
			enum class Kind { Value, Variable, Array, Resource } kind = Kind::Value;
			const Expression* expression = nullptr;
			const Binding* binding = nullptr;
			Id value = InvalidId;
			std::optional<std::string> tag;
			// A non-const local of this function, which a mutable reference can name.
			bool changeable = false;
			// A variable of this function's own storage: a reference to thread memory binds only to it.
			bool inThread = false;
			// A place in the constant address space (a constant buffer, a file-scope constant, or a
			// constant reference parameter of this function): the only thing a constant reference binds.
			bool inConstant = false;
			// Names a pointer or a buffer that is not a local array: not lowered as an argument.
			bool pointerLike = false;
		};
		OverloadArgument classifyArgument(const Expression& argument);
		// Qualification orders two references or two pointers of one conversion rank (the one to a
		// non-const object is better); -1 where it does not apply.
		struct ConversionCost { int rank = kNoConversion; int qualification = -1; };
		ConversionCost conversionCost(const OverloadArgument& argument, const Parameter& parameter);
		std::string linkageName(const FunctionDecl& helper) const {
			return _overloadedNames.count(helper.name) ? helperKey(helper, _overloadedNames) : helper.name;
		}
		Id helperFunctionId(HelperFunction& helper);
		std::pair<Stage, const FunctionDecl*> helperScope(const HelperFunction& helper) const;
		void emitLibraryHelpers();
		void emitImports();
		bool isExported(const FunctionDecl& helper) const;
		bool isMemberName(const std::string& name) const {
			const size_t tail = name.rfind("::");
			return tail != std::string::npos && _unit.findStruct(name.substr(0, tail));
		}
		void linkageAttributes(Id function, const std::string& name, uint32_t type);
		Id helperParameterType(const Type& type);
		void rejectAliasedReferenceArguments(const Expression& call, const FunctionDecl& definition,
			size_t first);
		HelperFunction& pointerSpecialisation(const Expression& call, HelperFunction& generic,
			size_t first, std::vector<Id>& arrays);
		// The pointer value a device or constant pointer argument passes: the whole
		// buffer its name binds, checked against the parameter's address space.
		Id bufferPointerArgument(const Expression& argument, const Parameter& parameter,
			const std::string& what);
		Id referenceArgument(const Expression& argument, const Parameter& parameter,
			const std::string& function, size_t position, std::vector<std::pair<Id, Id>>& copyOuts);
		Id resourceArgument(const Expression& argument, const Parameter& parameter,
			const std::string& function);
		Id resourcePointee(const Type& type);
		void emitHelper(const HelperFunction& helper);
		void emitHelperReturn(const Statement& statement);
		void emitVoidReturnValue(const Expression& expression, const std::string& quoted);
		// Set by emitVoidReturnValue for the one call it emits: a call to a helper returning void is then allowed.
		bool _voidCallAllowed = false;
		bool returnsVoid(const FunctionDecl& function) const;
		Id emitRoundHalfAway(Id value, Id type, uint32_t bits);
		Id emitSignExact(Id value, Id type, uint32_t bits);
		Id atomicAddress(const Expression& argument, const std::string& function, bool& isSigned);
		Id emitAtomicCall(const Expression& expression);
		Id emitAtomicCompareExchange(const Expression& expression);
		Id emitSelectCall(const Expression& expression);
		void emitBarrierCall(const Expression& expression);
		Id emitMathBuiltin(const MathBuiltin& builtin, const std::vector<ExpressionPtr>& arguments);
		Id emitConstruct(const Expression& expression);
		Id emitConstructList(const Expression& expression, Id toType);
		Id emitMatrixFromValue(const Expression& argument, const Type& target, Id toType,
			const std::string& spelled);
		Id emitInitListValue(const Type& target, const Expression& list);
		struct InitPlan;
		struct InitSlot {
			size_t index = 0;
			const InitializerElement* element = nullptr;
			std::shared_ptr<InitPlan> group;
		};
		struct InitPlan {
			const StructDecl* decl = nullptr;
			std::vector<InitSlot> slots;
		};
		struct DroppedInit {
			InitSlot slot;
			const Type* field;
		};
		void planStructElements(const StructDecl& decl, const std::vector<InitializerElement>& elements,
			size_t& cursor, std::vector<std::optional<Id>>& values, bool elided, InitPlan& plan,
			std::vector<DroppedInit>& dropped);
		Id emitInitSlot(const Type& field, const InitSlot& slot, std::vector<std::optional<Id>>& values);
		Id emitInitPlan(const Type& target, const InitPlan& plan, std::vector<std::optional<Id>>& values);
		Id emitStructElements(const Type& target, const StructDecl& decl, const std::vector<InitializerElement>& elements,
			std::vector<std::optional<Id>>& values);
		void discardLifted(const std::function<void()>& emit);
		Id emitListScalarValue(const Type& target, const Expression& expression, bool dropExcess = false);
		void checkAndDiscard(const Expression& expression);
		Id convertListScalar(Id value, const Type& target, const Expression& expression);
		bool canFoldExpression(const Expression& expression, const std::string& excluded = {});
		const StructDecl* structOf(const Expression& expression);
		Id emitSwizzle(const Expression& expression);
		Id emitIdentifier(const Expression& expression);
		Id loadFrom(Id pointer, Id pointeeType);
		Id promotedTo(uint32_t components) const;
		Id convert(Id value, Id fromType, Id toType);
		Id convertArray(Id value, Id fromType, Id toType);
		Id convertStruct(Id value, const TypeTable::StructForm& from, const TypeTable::StructForm& to, Id toType);
		Id composeStruct(Id type, const std::vector<uint32_t>& parts);
		Id constantStruct(Id type, const std::vector<uint32_t>& parts);
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

			if (from.declared[i] != to.declared[i] && _types.arrayElement(from.declared[i]) != InvalidId) {
				member = convertArray(member, from.declared[i], to.declared[i]);
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

	Id Emitter::emitIdentifier(const Expression& expression) {
		// The entry point's own names first, then the module's: a constant declared
		// at file scope is not a parameter of the entry point that reads it.
		const Binding* found = findResourceBinding(expression.name);
		if (!found) {
			if (_functionConstants.count(expression.name)) {
				return functionConstantValue(expression.name, false);
			}
			const auto alias = _functionConstantDefinedAliases.find(expression.name);
			if (alias != _functionConstantDefinedAliases.end()) {
				return functionConstantValue(alias->second, true);
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
	static std::optional<int64_t> literalIndex(const Expression& expression);

	// A name indexed down a chain, a[i][j], where the name is an array of more than one
	// dimension in the function's own storage or a file-scope constant array, and the chain
	// does not go past the array's dimensions. Anything deeper indexes an element, a vector.
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
		const auto it = _bindings.find(current->name);
		if (it == _bindings.end()) {
			return nullptr;
		}
		const Binding& binding = it->second;
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

	Id Emitter::emitIndex(const Expression& expression, bool asAddress) {
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
		const auto it = _bindings.find(name);
		if (!member && (it == _bindings.end() || !it->second.isPointer)) {
			throw CompileError("\"" + name + "\" is not a pointer, so it cannot "
				"be indexed");
		}

		const Binding& binding = member ? *member : it->second;

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

		const auto it = _bindings.find(expression.left->name);
		if (it == _bindings.end()) {
			return _constants.count(expression.left->name) > 0;
		}
		return !it->second.isBuffer && !_types.matrixInfo(it->second.pointeeType)
			&& _types.arrayElement(it->second.pointeeType) == InvalidId;
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
		bool isArrow = false;
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
	}

	// Distinguish struct members from vector swizzles before emitting the left side.
	static bool isTemplateMarker(const Type& type);

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

			// Either arm of c ? a : b names the struct both arms share.
			case ExpressionKind::Conditional: {
				const StructDecl* taken = structOf(*expression.arguments[0]);
				return taken ? taken : structOf(*expression.arguments[1]);
			}

			case ExpressionKind::Comma: return structOf(*expression.right);

			// S{...} is a value of the struct it names.
			case ExpressionKind::InitList:
				return expression.constructType && !expression.constructType->namedType.empty()
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
		if (indices < rank) {
			const Expression* base = &expression;
			while (base->kind == ExpressionKind::Index) {
				base = base->left.get();
			}
			const std::optional<Type> member = placeType(*base);
			if (!member || !member->isStdArray) {
				throw CompileError("an array member indexed this far is still an array, which is not a value: "
					"index it down to an element");
			}
		}
		Id fieldType = InvalidId;
		const Id address = emitMemberAddress(expression, fieldType);
		if (asAddress) {
			return address;
		}
		std::string rootName;
		std::vector<AccessStep> steps;
		accessChain(expression, rootName, steps);
		const auto root = _bindings.find(rootName);
		return root != _bindings.end() && (root->second.bufferPointeeType != InvalidId
				|| root->second.storageClass == spirv::StorageClass::PhysicalStorageBuffer)
			? loadFromBuffer(address, fieldType) : loadFrom(address, fieldType);
	}

	// A struct that is computed rather than named: a call, S{...}, a conditional or
	// comma of them, or a member of one. It has no address to walk, so its members
	// are taken from the value.
	static bool isStructValue(const Expression& expression) {
		switch (expression.kind) {
			case ExpressionKind::Call:
			case ExpressionKind::InitList:
			case ExpressionKind::Conditional:
			case ExpressionKind::Comma: return true;
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
			return _builder.emitTyped(spirv::OpCompositeExtract,
				declaredTypeOf(decl->fields[field].type), { value, static_cast<uint32_t>(field) });
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

	// float2x2(1.0f) is the diagonal matrix with 1.0 on it and 0 elsewhere, also for a
	// matrix that is not square, and float2x2(m) copies a matrix of the same shape,
	// converting its elements (measured with Apple's compiler and a GPU run). A matrix
	// of another shape is not a conversion Apple has either.
	Id Emitter::emitMatrixFromValue(const Expression& argument, const Type& target, Id toType,
		const std::string& spelled) {
		const Id value = emitExpression(argument);
		const Id fromType = _builder.typeOf(value);
		const auto* to = _types.matrixInfo(toType);

		if (const auto* from = _types.matrixInfo(fromType)) {
			if (from->columns != to->columns || from->rows != to->rows) {
				throw CompileError("no matching conversion from a matrix of another shape to " + spelled);
			}
			if (fromType == toType) {
				return value;
			}

			std::vector<uint32_t> columns;
			for (uint32_t column = 0; column < to->columns; ++column) {
				const Id piece = _builder.emitTyped(spirv::OpCompositeExtract, from->column, { value, column });
				columns.push_back(convert(piece, from->column, to->column));
			}
			return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
		}

		if (_types.vectorWidth(fromType) != 1 || _types.bitWidth(fromType) == 0) {
			throw CompileError("no matching conversion from this value to " + spelled
				+ "; a matrix is built from a scalar, a matrix of its own shape, or one vector per column");
		}

		const Id componentType = _types.componentOf(to->column);
		const Id diagonal = convert(value, fromType, componentType);
		const Id zero = _types.zero(componentType);
		std::vector<uint32_t> columns;
		for (uint32_t column = 0; column < to->columns; ++column) {
			std::vector<uint32_t> elements(to->rows, zero);
			if (column < to->rows) {
				elements[column] = diagonal;
			}
			columns.push_back(_builder.emitTyped(spirv::OpCompositeConstruct, to->column, elements));
		}
		return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
	}

	Id Emitter::emitConstruct(const Expression& expression) {
		if (!expression.constructType) {
			throw CompileError("a constructor needs the type it constructs");
		}

		const Type& target = *expression.constructType;
		const std::string spelled = typeName(target);

		if (isVoidCast(expression)) {
			throw CompileError("a cast to void has no value; it can only be a statement of its own, "
				"as in (void)x;");
		}

		// Only a scalar or a vector is a value a constructor produces. A pointer,
		// an array or a struct is not: "float* p(0)" is a null pointer in C++ and
		// not a pointer built from a value, and a local of a pointer type whose
		// initialiser is a scalar stores that scalar into the pointer, which
		// validates and reads as nonsense.
		if (target.isPointer || target.arrayLength || !target.namedType.empty()) {
			throw CompileError("constructing a " + spelled + " is not lowered yet; "
				"mslc builds a scalar or a vector value");
		}

		// Apple rejects device and constant on a local and takes thread (the function's own
		// storage, so a construct of one is plain) and threadgroup, which needs StorageClass
		// Workgroup. mslc declares these in Function storage, so
		// "threadgroup float3 v(0);" would be a thread-private local in a module
		// whose source says the value is shared, and that validates.
		if (target.addressSpace != AddressSpace::None && target.addressSpace != AddressSpace::Thread) {
			throw CompileError("a local in the " + std::string(addressSpaceName(target.addressSpace))
				+ " address space is not lowered yet; every local mslc declares is "
					"thread-private");
		}

		const Id toType = declaredTypeOf(target);

		// One vector per column, a single scalar for the diagonal, or one matrix of
		// the same shape. A list of scalars filling it element by element is its own
		// lowering and is reported rather than guessed at.
		if (target.isMatrix()) {
			if (expression.arguments.empty()) {
				return _types.zero(toType);
			}

			if (expression.arguments.size() == 1) {
				return emitMatrixFromValue(*expression.arguments.front(), target, toType, spelled);
			}

			// columns * rows scalars fill the columns in order. Apple takes the scalars alone,
			// not mixed with vectors, and converts each to the element type.
			if (expression.arguments.size() == static_cast<size_t>(target.matrixColumns) * target.vectorWidth) {
				const Id componentType = _types.componentOf(_types.matrixInfo(toType)->column);
				std::vector<uint32_t> scalars;
				for (const ExpressionPtr& argument: expression.arguments) {
					const Id value = emitExpression(*argument);
					const Id valueType = _builder.typeOf(value);
					if (_types.vectorWidth(valueType) != 1 || _types.bitWidth(valueType) == 0) {
						throw CompileError("no matching constructor for " + spelled + ": " + std::to_string(scalars.size() + 1)
							+ " of its " + std::to_string(expression.arguments.size()) + " values is not a scalar");
					}
					scalars.push_back(convert(value, valueType, componentType));
				}
				std::vector<uint32_t> columns;
				for (uint32_t column = 0; column < target.matrixColumns; ++column) {
					columns.push_back(_builder.emitTyped(spirv::OpCompositeConstruct,
						_types.matrixInfo(toType)->column,
						std::vector<uint32_t>(scalars.begin() + column * target.vectorWidth,
							scalars.begin() + (column + 1) * target.vectorWidth)));
				}
				return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
			}

			if (expression.arguments.size() != target.matrixColumns) {
				throw CompileError("no matching constructor for " + spelled + " built from "
					+ std::to_string(expression.arguments.size()) + " values");
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
				const auto local = expression.fileScope ? _bindings.end() : _bindings.find(expression.name);
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
					return initializer->kind != ExpressionKind::InitList && canFoldExpression(*initializer, global.name);
				}
				return false;
			}
			case ExpressionKind::Sizeof:
				try {
					sizeofOperand(*expression.left);
					return true;
				} catch (const CompileError&) {
					return false;
				}
			case ExpressionKind::Binary:
			case ExpressionKind::Conditional: return provenByFolding(expression, excluded);
			case ExpressionKind::Construct: {
				const Type* type = expression.constructType ? &*expression.constructType : nullptr;
				if (type && type->isVector() && !type->isMatrix() && !type->isPointer && !type->arrayLength
					&& type->namedType.empty() && type->enumName.empty()) {
					// An explicit conversion of each component; a float to an integer has a
					// range of its own to check, so that case is left unproven.
					for (const ExpressionPtr& argument: expression.arguments) {
						if (argument->kind == ExpressionKind::Construct && argument->constructType
							&& argument->constructType->isVector()) {
							if (!canFoldExpression(*argument, excluded)) { return false; }
							continue;
						}
						if (!canFoldExpression(*argument, excluded)) { return false; }
						const FoldedConstant value = foldExpression(*argument);
						if (value.isComposite || (isFloatKind(value.scalar) && !isFloatKind(type->scalar))) { return false; }
					}
					return !expression.arguments.empty();
				}
				return provenByFolding(expression, excluded);
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

	// What a constant expression is made of: constants at the leaves, and only the
	// operators that fold.
	bool Emitter::constantLeaves(const Expression& expression, const std::string& excluded) {
		switch (expression.kind) {
			case ExpressionKind::IntLiteral:
			case ExpressionKind::BoolLiteral:
			case ExpressionKind::FloatLiteral:
			case ExpressionKind::Sizeof:
			case ExpressionKind::Identifier: return canFoldExpression(expression, excluded);
			case ExpressionKind::Unary: return constantLeaves(*expression.left, excluded);
			case ExpressionKind::Binary:
				return constantLeaves(*expression.left, excluded) && constantLeaves(*expression.right, excluded);
			case ExpressionKind::Conditional: {
				if (expression.arguments.size() != 2 || !constantLeaves(*expression.left, excluded)) { return false; }
				const bool saved = _foldDomainError;
				bool taken = false;
				try {
					const FoldedConstant condition = foldExpression(*expression.left);
					taken = !condition.isComposite && (condition.scalar == ScalarKind::Bool ? condition.boolean
						: isFloatKind(condition.scalar) ? condition.number != 0.0 : condition.integer != 0);
				} catch (const CompileError&) {
					_foldDomainError = saved;
					return false;
				}
				_foldDomainError = saved;
				return constantLeaves(*expression.arguments[taken ? 0 : 1], excluded);
			}
			case ExpressionKind::Construct:
				for (const ExpressionPtr& argument: expression.arguments) {
					if (!constantLeaves(*argument, excluded)) { return false; }
				}
				return !expression.arguments.empty();
			default: return false;
		}
	}

	// Folds an expression of constants and says whether it is one: an operation no constant
	// expression holds (a signed overflow, a shift out of range, a division by zero) or a
	// form the folder does not know is not proven.
	bool Emitter::provenByFolding(const Expression& expression, const std::string& excluded) {
		if (!constantLeaves(expression, excluded)) { return false; }
		const bool saved = _foldDomainError;
		_foldDomainError = false;
		bool proven = true;
		try {
			foldExpression(expression);
		} catch (const CompileError&) {
			proven = false;
		}
		proven = proven && !_foldDomainError;
		_foldDomainError = saved;
		return proven;
	}

	Id Emitter::convertListScalar(Id value, const Type& target, const Expression& expression) {
		if (!target.enumName.empty()) {
			requireEnumValue(target.enumName, expression, "an element of the initializer list");
		}
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

	void Emitter::discardLifted(const std::function<void()>& emit) {
		const size_t mark = _builder.functionsMark();
		const size_t locals = _builder.localsMark();
		const Id block = _currentBlock;
		const bool terminated = _terminated;
		const uint32_t depth = _controlDepth;
		const size_t targets = _jumpTargets.size();
		// what the lifted code reads is not read
		const auto functionConstantReads = _functionConstantReads;
		emit();
		_builder.takeFunctionsFrom(mark);
		_builder.truncateLocals(locals);
		_functionConstantReads = functionConstantReads;
		// A short-circuit or ?: inside the element moved these to a label that the lift removed.
		_currentBlock = block;
		_terminated = terminated;
		_controlDepth = depth;
		_jumpTargets.resize(targets);
	}

	// Apple type-checks an ignored initializer element but never evaluates it, so
	// it is emitted for its diagnostics and the instructions are lifted out again.
	void Emitter::checkAndDiscard(const Expression& expression) {
		if (expression.kind == ExpressionKind::InitList) {
			for (const InitializerElement& element: expression.elements) { checkAndDiscard(*element.value); }
			return;
		}
		discardLifted([&] { emitExpression(expression); });
	}

	Id Emitter::emitListScalarValue(const Type& target, const Expression& expression, bool dropExcess) {
		if (expression.kind != ExpressionKind::InitList) {
			return convertListScalar(emitExpression(expression), target, expression);
		}
		if (expression.elements.empty()) { return _types.zero(declaredTypeOf(target)); }
		if (expression.elements.size() != 1 && !dropExcess) {
			throw CompileError("nested scalar initialization with excess elements is not lowered yet");
		}
		if (!expression.elements.front().fieldName.empty()) {
			throw CompileError("a field designator requires a struct initializer");
		}
		Id first = emitListScalarValue(target, *expression.elements.front().value, dropExcess);
		for (size_t i = 1; i < expression.elements.size(); ++i) { checkAndDiscard(*expression.elements[i].value); }
		return first;
	}

	// Apple keeps the last initializer of each member, drops the earlier ones unevaluated (but still
	// type-checked) and evaluates the survivors in member order, whatever order they were written in.
	// A flat cursor over the element list places each element; a struct member that is reached by an
	// element which is neither braced nor already of the member's struct type takes the following
	// elements too (brace elision).
	void Emitter::planStructElements(const StructDecl& decl, const std::vector<InitializerElement>& elements,
			size_t& cursor, std::vector<std::optional<Id>>& values, bool elided, InitPlan& plan,
			std::vector<DroppedInit>& dropped) {
		const std::string spelled = decl.name;
		plan.decl = &decl;
		plan.slots.resize(decl.fields.size());
		size_t next = 0;
		while (cursor < elements.size()) {
			const InitializerElement& element = elements[cursor];
			if (!element.fieldName.empty()) {
				if (elided) { break; }
				next = 0;
				while (next < decl.fields.size() && decl.fields[next].name != element.fieldName) { ++next; }
				if (next == decl.fields.size()) {
					throw CompileError("field " + element.fieldName + " is unknown in " + spelled + " initializer");
				}
			}
			if (next == decl.fields.size()) {
				if (elided) { break; }
				throw CompileError("excess elements in " + spelled + " initializer");
			}
			const Type& field = decl.fields[next].type;
			if (field.arrayLength && element.value->kind != ExpressionKind::InitList) {
				throw CompileError("brace elision into the array member " + decl.fields[next].name + " of " + spelled
					+ " is not lowered yet; add the braces");
			}
			InitSlot slot;
			slot.index = cursor;
			const StructDecl* fieldDecl = field.namedType.empty() || field.arrayLength.has_value()
				|| element.value->kind == ExpressionKind::InitList || !element.fieldName.empty()
				? nullptr : _unit.findStruct(field.namedType);
			if (fieldDecl && !fieldDecl->hasConstructors) {
				Id sourceType = InvalidId;
				const ExpressionKind kind = element.value->kind;
				const bool literal = kind == ExpressionKind::IntLiteral || kind == ExpressionKind::FloatLiteral
					|| kind == ExpressionKind::BoolLiteral;
				if (values[cursor]) { sourceType = _builder.typeOf(*values[cursor]); }
				else if (!literal) { discardLifted([&] { sourceType = _builder.typeOf(emitExpression(*element.value)); }); }
				const std::string* sourceName = _types.structNameOf(sourceType, nullptr);
				if (!(sourceName && *sourceName == field.namedType)) {
					if (fieldDecl->fields.empty()) {
						throw CompileError("initializer for " + typeName(field) + ", which has no members, requires explicit braces");
					}
					if (std::any_of(fieldDecl->fields.begin(), fieldDecl->fields.end(),
							[](const StructField& member) { return member.type.arrayLength.has_value(); })) {
						throw CompileError("brace elision into " + typeName(field) + ", which has an array member, "
							"is not lowered yet; add the braces");
					}
					slot.group = std::make_shared<InitPlan>();
					planStructElements(*fieldDecl, elements, cursor, values, true, *slot.group, dropped);
				}
			}
			if (!slot.group) {
				slot.element = &element;
				++cursor;
			}
			if (plan.slots[next].element || plan.slots[next].group) {
				dropped.push_back({plan.slots[next], &field});
			}
			plan.slots[next] = std::move(slot);
			++next;
		}
	}

	Id Emitter::emitInitSlot(const Type& field, const InitSlot& slot, std::vector<std::optional<Id>>& values) {
		if (slot.group) { return emitInitPlan(field, *slot.group, values); }
		const Expression& expression = *slot.element->value;
		if (expression.kind == ExpressionKind::InitList) { return emitInitListValue(field, expression); }
		if (!values[slot.index]) { values[slot.index] = emitExpression(expression); }
		const Id value = *values[slot.index];
		return field.namedType.empty() && field.isScalar()
			? convertListScalar(value, field, expression)
			: convertImplicit(value, declaredTypeOf(field));
	}

	Id Emitter::emitInitPlan(const Type& target, const InitPlan& plan, std::vector<std::optional<Id>>& values) {
		Expression empty;
		empty.kind = ExpressionKind::InitList;
		std::vector<uint32_t> fields;
		for (size_t i = 0; i < plan.decl->fields.size(); ++i) {
			const Type& field = plan.decl->fields[i].type;
			const InitSlot& slot = plan.slots[i];
			fields.push_back(slot.element || slot.group ? emitInitSlot(field, slot, values) : emitInitListValue(field, empty));
		}
		return composeStruct(declaredTypeOf(target), fields);
	}

	Id Emitter::emitStructElements(const Type& target, const StructDecl& decl,
			const std::vector<InitializerElement>& elements, std::vector<std::optional<Id>>& values) {
		size_t cursor = 0;
		InitPlan plan;
		std::vector<DroppedInit> dropped;
		planStructElements(decl, elements, cursor, values, false, plan, dropped);
		const Id composite = emitInitPlan(target, plan, values);
		for (const DroppedInit& drop: dropped) {
			discardLifted([&] { emitInitSlot(*drop.field, drop.slot, values); });
		}
		return composite;
	}

	Id Emitter::emitInitListValue(const Type& target, const Expression& list) {
		if (target.addressSpace != AddressSpace::None) {
			throw CompileError("a local in the " + std::string(addressSpaceName(target.addressSpace))
				+ " address space is not lowered yet; every local mslc declares is thread-private");
		}
		if (target.arrayLength) { return emitArrayListValue(target, list); }
		const Id toType = declaredTypeOf(target);
		const std::string spelled = typeName(target);
		if (!target.namedType.empty()) {
			const StructDecl& decl = *_unit.findStruct(target.namedType);
			if (decl.hasConstructors) {
				throw CompileError("brace initialization of " + spelled + " with declared constructors is not lowered yet");
			}
			const bool hasArrayMember = std::any_of(decl.fields.begin(), decl.fields.end(),
				[](const StructField& field) { return field.type.arrayLength.has_value(); });
			if (hasArrayMember && list.elements.empty()) {
				return _types.zero(toType);
			}
			std::vector<std::optional<Id>> values(list.elements.size());
			// S s{S{1,2}} copies the temporary; a bare {1,2} there would initialise the first member.
			const Expression* only = list.elements.size() == 1 ? list.elements.front().value.get() : nullptr;
			if (only && list.elements.front().fieldName.empty() && only->kind == ExpressionKind::InitList
				&& only->constructType && only->constructType->namedType == target.namedType) {
				return convertImplicit(emitExpression(*only), toType);
			}
			if (list.elements.size() == 1 && list.elements.front().fieldName.empty()
				&& list.elements.front().value->kind != ExpressionKind::InitList) {
				values[0] = emitExpression(*list.elements.front().value);
				const std::string* sourceName = _types.structNameOf(_builder.typeOf(*values[0]), nullptr);
				if (sourceName && *sourceName == target.namedType) {
					return convertImplicit(*values[0], toType);
				}
			}
			return emitStructElements(target, decl, list.elements, values);
		}
		if (list.elements.empty()) { return _types.zero(toType); }
		for (const InitializerElement& element: list.elements) {
			if (!element.fieldName.empty()) { throw CompileError("a field designator requires a struct initializer"); }
		}
		if (target.isMatrix()) {
			if (list.elements.size() > target.matrixColumns) {
				throw CompileError("excess elements in a matrix initializer; Apple ignores them, "
					"mslc asks for one per column");
			}
			Type columnType;
			columnType.scalar = target.scalar;
			columnType.vectorWidth = target.vectorWidth;
			const Id columnId = declaredTypeOf(columnType);
			std::vector<uint32_t> columns;
			for (const InitializerElement& element: list.elements) {
				if (element.value->kind == ExpressionKind::InitList && element.value->elements.empty()) {
					throw CompileError("an empty matrix column initializer is not lowered yet");
				}
				if (element.value->kind == ExpressionKind::InitList
					&& element.value->elements.size() != target.vectorWidth) {
					throw CompileError("a column of " + typeName(target) + " takes "
						+ std::to_string(target.vectorWidth) + " values in braces");
				}
				Id column = element.value->kind == ExpressionKind::InitList
					? emitInitListValue(columnType, *element.value) : emitExpression(*element.value);
				// A scalar fills the whole column, as it does in Apple's compiler.
				if (_types.scalarKindOf(_builder.typeOf(column)) && element.value->kind != ExpressionKind::InitList) {
					Type component = columnType;
					component.vectorWidth = 0;
					column = broadcast(convertListScalar(column, component, *element.value), columnId);
				}
				if (_builder.typeOf(column) != columnId) {
					throw CompileError("a matrix initializer column has to have its column type already");
				}
				columns.push_back(column);
			}
			while (columns.size() < target.matrixColumns) {
				columns.push_back(_types.zero(columnId));
			}
			return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
		}
		Type component;
		component.scalar = target.scalar;
		component.enumName = target.enumName;
		if (target.isScalar()) {
			if (list.elements.size() != 1) { throw CompileError("excess elements in a scalar initializer list"); }
			return emitListScalarValue(component, *list.elements.front().value, true);
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
		if (expression.unaryOperator == UnaryOperator::AddressOf) {
			throw CompileError("taking an address with '&' is not supported; only an atomic_*_explicit "
				"function takes the address of an element");
		}
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

		// The integer promotion: + - ~ on a scalar narrower than int give an int (! gives a bool; a
		// vector keeps its type, as Apple does), so an overload sees an int however the value is used.
		const bool promotes = expression.unaryOperator != UnaryOperator::Not && !_types.isFloat(type)
			&& _types.vectorWidth(type) == 1 && _types.bitWidth(type) < 32;
		if (promotes) {
			const Id promoted = promotedTo(1);
			return emitUnaryOperation(expression.unaryOperator, convert(operand, type, promoted), promoted);
		}
		return emitUnaryOperation(expression.unaryOperator, operand, type);
	}

	Id Emitter::emitUnaryOperation(UnaryOperator op, Id operand, Id type) {
		switch (op) {
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
				if (_types.isFloat(type)) { throw CompileError("operator ~ needs an integer operand"); }
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
			case ExpressionKind::Sizeof:
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
			case ExpressionKind::Comma:
				return safe(expression.left) && safe(expression.right);
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
		const bool isComparison = !isLogical && isComparisonOperator(op);

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

		// Two bool operands compare by value, as OpLogicalEqual does for scalars.
		const bool logical = (op == BinaryOperator::Equal || op == BinaryOperator::NotEqual)
			&& _types.isBool(leftType) && _types.isBool(rightType);

		if (_types.isBool(leftType) != _types.isBool(rightType)
			&& _types.vectorWidth(leftType) > 1 && _types.vectorWidth(rightType) > 1) {
			throw CompileError("implicit conversions between vector types are not permitted");
		}

		for (const Id type: { leftType, rightType }) {
			if (!logical && (_types.bitWidth(type) == 0 || _types.isBool(type))) {
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
		if (logical) {
			return _builder.emitTyped(op == BinaryOperator::Equal
				? spirv::OpLogicalEqual : spirv::OpLogicalNotEqual, resultType, { left, right });
		}

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

	// The atomic functions of Metal that take the pointer of a device atomic_uint or
	// atomic_int, with the memory order spelled out. Only the relaxed order exists.
	static bool isAtomicFunction(const std::string& name) {
		static const char* const names[] = {
			"atomic_fetch_add_explicit", "atomic_fetch_sub_explicit", "atomic_fetch_min_explicit",
			"atomic_fetch_max_explicit", "atomic_fetch_and_explicit", "atomic_fetch_or_explicit",
			"atomic_fetch_xor_explicit", "atomic_exchange_explicit", "atomic_load_explicit",
			"atomic_store_explicit", "atomic_compare_exchange_weak_explicit",
			"atomic_compare_exchange_strong_explicit",
		};
		return std::find(std::begin(names), std::end(names), name) != std::end(names);
	}

	Id Emitter::atomicAddress(const Expression& argument, const std::string& function, bool& isSigned) {
		const Expression* pointer = &argument;
		const Expression* index = nullptr;
		bool addressOfLocal = false;
		if (argument.kind == ExpressionKind::Unary && argument.unaryOperator == UnaryOperator::AddressOf) {
			const Expression& element = *argument.left;
			if (element.kind == ExpressionKind::Identifier) {
				pointer = &element;
				addressOfLocal = true;
			} else if (element.kind == ExpressionKind::Index && !element.isDereference
				&& element.left->kind == ExpressionKind::Identifier && element.arguments.size() == 1) {
				pointer = element.left.get();
				index = element.arguments[0].get();
			} else {
				throw CompileError(function + " takes &local for a threadgroup atomic, or &counters[i] for an atomic buffer");
			}
		}

		if (pointer->kind != ExpressionKind::Identifier) {
			throw CompileError(function + " takes &local for a scalar threadgroup atomic, an atomic buffer "
				"pointer, or the address of one of its elements");
		}
		const auto found = _bindings.find(pointer->name);
		if (found == _bindings.end() || !found->second.pointeeMsl.isAtomic
			|| (!found->second.isBuffer && found->second.storageClass != spirv::StorageClass::Workgroup)) {
			throw CompileError(function + " takes a device atomic_uint or atomic_int pointer, or &local for a "
				"scalar threadgroup atomic, and \""
				+ pointer->name + "\" is not one");
		}

		const Binding& binding = found->second;
		if (binding.storageClass == spirv::StorageClass::Workgroup && (!addressOfLocal || index)) {
			throw CompileError(function + " takes the address of a scalar threadgroup atomic, as &" + pointer->name);
		}
		if (binding.isBuffer && addressOfLocal) {
			throw CompileError(function + " takes the address of an element of an atomic buffer, as &counters[i]");
		}
		if (binding.pointeeMsl.addressSpace == AddressSpace::Constant) {
			throw CompileError(function + " takes a device atomic, and \"" + pointer->name
				+ "\" points into the constant address space");
		}
		if (binding.readOnly && function != "atomic_load_explicit") {
			throw CompileError(function + " changes the atomic, and \"" + pointer->name
				+ "\" points at a constant or const one; only atomic_load_explicit reads it");
		}
		isSigned = binding.pointeeMsl.scalar == ScalarKind::Int;
		if (binding.storageClass == spirv::StorageClass::Workgroup) {
			return binding.id;
		}
		const Id indexId = index ? stepIndex(*index, "buffer element") : constantU32(0);
		return _builder.emitTyped(spirv::OpAccessChain,
			_types.pointer(spirv::StorageClass::PhysicalStorageBuffer, binding.pointeeType),
			{ bufferBase(binding), constantU32(0), indexId });
	}

	static std::string builtinCalleeName(const std::string& name) {
		constexpr std::string_view prefix = "metal::";
		if (name.compare(0, prefix.size(), prefix) == 0
			&& name.find("::", prefix.size()) == std::string::npos) {
			return name.substr(prefix.size());
		}
		return name;
	}

	Id Emitter::emitAtomicCall(const Expression& expression) {
		const std::string name = builtinCalleeName(expression.left->name);
		const bool isStore = name == "atomic_store_explicit";
		const bool isLoad = name == "atomic_load_explicit";
		if (name == "atomic_compare_exchange_strong_explicit") {
			throw CompileError(name + " is not a Metal function; Metal declares only "
				"atomic_compare_exchange_weak_explicit");
		}
		if (name == "atomic_compare_exchange_weak_explicit") {
			return emitAtomicCompareExchange(expression);
		}

		const size_t expected = isLoad ? 2 : 3;
		if (expression.arguments.size() != expected) {
			throw CompileError(name + " takes " + std::to_string(expected) + " arguments: the pointer, "
				+ (isLoad ? "" : "the value, ") + "and memory_order_relaxed");
		}
		const Expression& order = *expression.arguments.back();
		if (order.kind != ExpressionKind::Identifier
			|| (order.name != "memory_order_relaxed" && order.name != "metal::memory_order_relaxed")) {
			throw CompileError(name + " takes memory_order_relaxed, the only memory order Metal has");
		}

		bool isSigned = false;
		const Id address = atomicAddress(*expression.arguments[0], name, isSigned);
		const Id type = isSigned ? _intType : _uintType;
		const Id scope = constantU32(_types.storageClassOf(_builder.typeOf(address)) == spirv::StorageClass::Workgroup ? 2 : 1);
		const Id semantics = constantU32(0);  // Relaxed

		if (isLoad) {
			return _builder.emitTyped(spirv::OpAtomicLoad, type, { address, scope, semantics });
		}

		const Id value = convertImplicit(emitExpression(*expression.arguments[1]), type);
		if (isStore) {
			_builder.emit(spirv::OpAtomicStore, { address, scope, semantics, value });
			return InvalidId;
		}

		uint16_t opcode = spirv::OpAtomicIAdd;
		if (name == "atomic_fetch_sub_explicit") opcode = spirv::OpAtomicISub;
		else if (name == "atomic_fetch_min_explicit") opcode = isSigned ? spirv::OpAtomicSMin : spirv::OpAtomicUMin;
		else if (name == "atomic_fetch_max_explicit") opcode = isSigned ? spirv::OpAtomicSMax : spirv::OpAtomicUMax;
		else if (name == "atomic_fetch_and_explicit") opcode = spirv::OpAtomicAnd;
		else if (name == "atomic_fetch_or_explicit") opcode = spirv::OpAtomicOr;
		else if (name == "atomic_fetch_xor_explicit") opcode = spirv::OpAtomicXor;
		else if (name == "atomic_exchange_explicit") opcode = spirv::OpAtomicExchange;
		return _builder.emitTyped(opcode, type, { address, scope, semantics, value });
	}

	// bool atomic_compare_exchange_weak_explicit(pointer, &expected, desired, relaxed, relaxed).
	// OpAtomicCompareExchange returns what the atomic held; that equals expected exactly when
	// the exchange happened, and expected takes it otherwise, so storing it back unconditionally
	// is the same as Metal's write on failure.
	Id Emitter::emitAtomicCompareExchange(const Expression& expression) {
		const std::string name = "atomic_compare_exchange_weak_explicit";
		if (expression.arguments.size() != 5) {
			throw CompileError(name + " takes 5 arguments: the pointer, &expected, the desired value, and "
				"memory_order_relaxed twice");
		}
		for (size_t i = 3; i < 5; ++i) {
			const Expression& order = *expression.arguments[i];
			if (order.kind != ExpressionKind::Identifier
				|| (order.name != "memory_order_relaxed" && order.name != "metal::memory_order_relaxed")) {
				throw CompileError(name + " takes memory_order_relaxed, the only memory order Metal has");
			}
		}

		bool isSigned = false;
		const Id address = atomicAddress(*expression.arguments[0], name, isSigned);
		const Id type = isSigned ? _intType : _uintType;

		const Expression& expectedArgument = *expression.arguments[1];
		const Expression* expected = expectedArgument.kind == ExpressionKind::Unary
			&& expectedArgument.unaryOperator == UnaryOperator::AddressOf
			&& expectedArgument.left->kind == ExpressionKind::Identifier ? expectedArgument.left.get() : nullptr;
		if (!expected) {
			throw CompileError(name + " takes &expected, the address of a local scalar variable, as its "
				"second argument");
		}
		const auto found = _bindings.find(expected->name);
		if (found == _bindings.end() || !found->second.isPointer || found->second.isBuffer
			|| found->second.isAddress || found->second.storageClass != spirv::StorageClass::Function
			|| found->second.bufferPointeeType != InvalidId) {
			throw CompileError(name + " takes &expected, the address of a local scalar variable, and \""
				+ expected->name + "\" is not one");
		}
		const Type& expectedType = found->second.pointeeMsl;
		if (found->second.readOnly) {
			throw CompileError(name + " writes the old value to \"" + expected->name + "\", which is const");
		}
		if (expectedType.isAtomic || expectedType.isPointer || !expectedType.isScalar() || expectedType.isMatrix()
			|| !expectedType.namedType.empty() || !expectedType.enumName.empty() || expectedType.arrayLength
			|| expectedType.scalar != (isSigned ? ScalarKind::Int : ScalarKind::UInt)) {
			throw CompileError(name + " takes &expected of the type the atomic holds, " + (isSigned ? "int" : "uint")
				+ ", and \"" + expected->name + "\" is not one");
		}

		const Id desired = convertImplicit(emitExpression(*expression.arguments[2]), type);
		const Id comparator = emitExpression(*expected);
		const Id scope = constantU32(_types.storageClassOf(_builder.typeOf(address)) == spirv::StorageClass::Workgroup ? 2 : 1);
		const Id semantics = constantU32(0);  // Relaxed
		const Id old = _builder.emitTyped(spirv::OpAtomicCompareExchange, type,
			{ address, scope, semantics, semantics, desired, comparator });
		_builder.emit(spirv::OpStore, { found->second.id, old });
		return _builder.emitTyped(spirv::OpIEqual, _boolType, { old, comparator });
	}

	// The mem_flags a barrier names, as the SPIR-V memory semantics they select, and
	// the widest scope among them. Metal's mem_flags is an enum class, so only the
	// qualified spelling joined with | is a flag set.
	static void collectMemFlags(const Expression& flags, const std::string& function,
		uint32_t& semantics, bool& crossesWorkgroup) {
		if (flags.kind == ExpressionKind::Binary && flags.binaryOperator == BinaryOperator::BitOr) {
			collectMemFlags(*flags.left, function, semantics, crossesWorkgroup);
			collectMemFlags(*flags.right, function, semantics, crossesWorkgroup);
			return;
		}
		const std::string prefix = flags.name.rfind("metal::mem_flags::", 0) == 0
			? "metal::mem_flags::" : "mem_flags::";
		if (flags.kind != ExpressionKind::Identifier || flags.name.rfind(prefix, 0) != 0) {
			throw CompileError(function + " takes mem_flags::mem_none, mem_device, mem_threadgroup or "
				"mem_texture, joined with |");
		}
		const std::string flag = flags.name.substr(prefix.size());
		if (flag == "mem_none") {
			return;
		}
		if (flag == "mem_device") {
			semantics |= 64;  // UniformMemory, which includes buffers
			crossesWorkgroup = true;
		} else if (flag == "mem_threadgroup") {
			semantics |= 256;  // WorkgroupMemory
		} else if (flag == "mem_texture") {
			semantics |= 2048;  // ImageMemory
			crossesWorkgroup = true;
		} else {
			throw CompileError(function + " does not lower mem_flags::" + flag + "; mem_none, mem_device, "
				"mem_threadgroup and mem_texture are the flags mslc supports");
		}
	}

	void Emitter::emitBarrierCall(const Expression& expression) {
		const std::string name = builtinCalleeName(expression.left->name);
		if (expression.arguments.size() != 1) {
			throw CompileError(name + " takes one argument: the mem_flags to order");
		}
		if (_entryPoint->stage != Stage::Kernel) {
			throw StageError(name + " is only supported in a kernel function; Vulkan allows a "
				"control barrier only in compute shaders");
		}

		uint32_t semantics = 0;
		bool crossesWorkgroup = false;
		collectMemFlags(*expression.arguments.front(), name, semantics, crossesWorkgroup);
		if (semantics != 0) {
			semantics |= 8;  // AcquireRelease
		}

		const bool isSimd = name == "simdgroup_barrier";
		const uint32_t executionScope = isSimd ? 3 : 2;  // Subgroup : Workgroup
		const uint32_t memoryScope = isSimd ? 3 : (crossesWorkgroup ? 1 : 2);  // Device : Workgroup
		_builder.emit(spirv::OpControlBarrier, { constantU32(executionScope), constantU32(memoryScope),
			constantU32(semantics) });
	}

	Id Emitter::emitCall(const Expression& expression) {
		// Taken before any argument is evaluated, so only this call, and none in its arguments, may be void.
		const bool voidAllowed = std::exchange(_voidCallAllowed, false);
		const Expression* receiver = nullptr;
		std::vector<Id> prepared;
		HelperFunction* helper = findHelper(expression, receiver, prepared);
		if (!helper && expression.left->kind == ExpressionKind::Member) {
			return emitTextureCall(expression);
		}

		if (!helper && expression.left->kind != ExpressionKind::Identifier) {
			throw CompileError("only a direct function call is supported");
		}

		if (helper) {
			if (returnsVoid(*helper->definition) && !voidAllowed) {
				throw CompileError("a call to \"" + helper->definition->name + "\" returns void and has "
					"no value");
			}

			return emitHelperCall(expression, *helper, receiver, prepared);
		}

		const std::string name = builtinCalleeName(expression.left->name);
		if (name == "threadgroup_barrier" || name == "simdgroup_barrier") {
			throw CompileError(name + " returns void and has no value");
		}

		if (name == "__metal_is_function_constant_defined") {
			return emitIsFunctionConstantDefined(expression);
		}

		if (isAtomicFunction(name)) {
			if (name == "atomic_store_explicit") {
				throw CompileError("atomic_store_explicit returns void and has no value");
			}
			return emitAtomicCall(expression);
		}

		if (name == "select") {
			return emitSelectCall(expression);
		}

		const MathBuiltin* builtin = findMathBuiltin(name);
		if (!builtin) {
			throw CompileError("function \"" + expression.left->name + "\" is not a builtin mslc "
				"recognises, and it is not a function declared in this file");
		}

		return emitMathBuiltin(*builtin, expression.arguments);
	}

	// select(a, b, c): b where c holds, else a. Apple's library declares it only for a
	// bool or boolN condition, never an integer vector, and a scalar condition beside
	// vector operands applies to every lane (a numeric scalar converts as in an if).
	// The operands are one type, or a scalar beside a vector, which is broadcast;
	// two scalars of different types are ambiguous there, and so are they here.
	Id Emitter::emitSelectCall(const Expression& expression) {
		if (expression.arguments.size() != 3) {
			throw CompileError("select takes 3 arguments, and this call passes "
				+ std::to_string(expression.arguments.size()));
		}

		Id values[3];
		for (size_t i = 0; i < 3; ++i) {
			values[i] = emitExpression(*expression.arguments[i]);
		}

		for (size_t i = 0; i < 2; ++i) {
			const Id type = _builder.typeOf(values[i]);
			if (_types.matrixInfo(type) || _types.bitWidth(type) == 0) {
				throw CompileError("select takes bool, numeric or vector operands");
			}
		}

		Id aType = _builder.typeOf(values[0]);
		Id bType = _builder.typeOf(values[1]);
		const uint32_t aWidth = _types.vectorWidth(aType);
		const uint32_t bWidth = _types.vectorWidth(bType);
		const Id conditionType = _builder.typeOf(values[2]);
		const uint32_t conditionWidth = _types.vectorWidth(conditionType);

		if (aWidth == 1 && bWidth == 1) {
			if (aType != bType) {
				throw CompileError("the operands of select have to be one type: Apple reports a call "
					"with two scalars of different types as ambiguous");
			}
			if (conditionWidth > 1) {
				throw CompileError("select with a vector condition takes vector operands");
			}
		} else if (aWidth > 1 && bWidth > 1) {
			if (aType != bType) {
				throw CompileError("the operands of select have to be one type, or a scalar beside a vector");
			}
		} else if (aWidth == 1) {
			values[0] = broadcast(values[0], bType);
			aType = bType;
		} else {
			values[1] = broadcast(values[1], aType);
			bType = aType;
		}

		const uint32_t width = _types.vectorWidth(aType);
		Id condition;
		if (conditionWidth > 1) {
			if (_types.componentOf(conditionType) != _boolType || conditionWidth != width) {
				throw CompileError("the condition of select beside " + std::to_string(width)
					+ "-component operands has to be a bool or a bool" + std::to_string(width));
			}
			condition = values[2];
		} else {
			condition = asCondition(values[2]);
		}

		return _builder.emitTyped(spirv::OpSelect, aType, { condition, values[1], values[0] });
	}

	// The helper a call names. A call through "." is a member function when the
	// receiver is a struct value, and then receiver is the expression to be passed
	// first.
	Emitter::HelperFunction* Emitter::findHelper(const Expression& call, const Expression*& receiver,
		std::vector<Id>& prepared) {
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
			// A name only a member template has is looked up through the receiver's struct, which structOf
			// reads without emitting anything, so a call such as a texture's own method is left alone.
			const bool templateNamed = _memberTemplateNames.count(member.memberName)
				&& std::none_of(_helpers.begin(), _helpers.end(), isMember)
				&& [&] {
					const StructDecl* owner = structOf(*member.left);
					return owner && !memberTemplatesOf(owner->name, member.memberName).empty();
				}();
			if ((texture != _bindings.end() && texture->second.pointeeMsl.isTexture())
				|| (std::none_of(_helpers.begin(), _helpers.end(), isMember) && !templateNamed)) {
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
			if (const std::vector<const FunctionTemplate*> templates = memberTemplatesOf(ownerName, member.memberName);
				!templates.empty()) {
				if (found != _helpers.end()) {
					throw CompileError("\"" + ownerName + suffix + "\" is both a member function and a member function "
						"template, which Apple takes as one overload set and mslc does not lower yet");
				}
				// Apple takes a member function on a device or constant object only when it is qualified so.
				if (const OverloadArgument object = classifyArgument(*member.left);
					object.kind == OverloadArgument::Kind::Variable && !object.inThread) {
					throw CompileError("the member function template \"" + ownerName + suffix + "\" is called on an object "
						"in device, constant or threadgroup memory, which Apple rejects (a member function is for a "
						"thread object); copy the object to a local first");
				}
				receiver = member.left.get();
				return resolveOverload(call, {}, templates, {}, prepared, receiver);
			}
			if (found != _helpers.end() && !member.templateArguments.empty()) {
				throw CompileError("\"" + found->first + "\" is not a template, and the call gives it explicit template "
					"arguments, which Apple rejects (another struct declares a member template of that name)");
			}
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

			receiver = member.left.get();
			return &found->second;
		}

		if (call.left->kind != ExpressionKind::Identifier) {
			return nullptr;
		}

		std::set<std::string> adl;
		const std::vector<std::string> names = namesOfCall(call, &adl);
		std::vector<HelperFunction*> set;
		for (const std::string& name: names) {
			const std::vector<HelperFunction*> named = helpersNamed(name);
			set.insert(set.end(), named.begin(), named.end());
		}
		// A template is an overload of a helper of its name, so the arguments choose between them.
		const std::vector<const FunctionTemplate*> templates = visibleTemplates(names, adl);
		// Apple binds a call that does not depend on the template's parameters where the template is declared,
		// so a function declared below it, found only by the end-of-file lookup, must not take such a call.
		if (isInstance(_helper) && !adl.empty() && !dependsOnTemplate(call)) {
			const auto below = std::any_of(set.begin(), set.end(), [&](const HelperFunction* entry) {
				return entry->firstOrder > callerOrder() && adl.count(entry->definition->name);
			}) || std::any_of(templates.begin(), templates.end(), [&](const FunctionTemplate* declaration) {
				return declaration->order > callerOrder() && adl.count(declaration->name);
			});
			if (below) {
				throw CompileError("a call inside a template whose arguments do not depend on its parameters finds a "
					"function declared below the template through argument-dependent lookup; Apple binds such a call "
					"where the template is declared, which is not lowered yet");
			}
		}
		// validateHelpers checked the calls of a plain helper by name (arity, declared above, defined);
		// an instance's calls are checked by resolving them.
		if (!templates.empty() || call.left->hasTemplateArguments || set.size() > 1
			|| (isInstance(_helper) && !set.empty())) {
			return resolveOverload(call, set, templates, adl, prepared);
		}
		// One function found among several names (argument-dependent lookup found the only one) is
		// still checked as a chosen one; a name found alone was checked by validateHelpers.
		return set.empty() ? nullptr : names.size() > 1 ? definedHelper(set.front()) : set.front();
	}

	// A function a call selects has to have a body here, or be one the unit imports.
	Emitter::HelperFunction* Emitter::definedHelper(HelperFunction* entry) {
		if (!entry->definition->body && !_options.linkageObject) {
			throw CompileError("\"" + entry->definition->name + "(" + parameterSignature(*entry->definition)
				+ ")\" is declared but never defined");
		}
		// The scan for imports does not know the functions argument-dependent lookup adds.
		if (!entry->definition->body && entry->ids.empty()) {
			throw CompileError("\"" + entry->definition->name + "\" is declared but defined elsewhere, and "
				"this call finds it by argument-dependent lookup, which importing does not follow yet");
		}
		return entry;
	}

	// The names a call can denote: the one the parser resolved, the others it found at the same
	// level (using directives and declarations), and for an unqualified call the same-named
	// function of the namespace of each struct argument (argument-dependent lookup).
	// The struct an argument has the type of, for argument-dependent lookup: structOf, and the
	// forms that only keep their operand's type (a comma, a conditional).
	// Kept apart from structOf, which member access calls on any expression.
	const StructDecl* Emitter::adlStructOf(const Expression& argument) {
		switch (argument.kind) {
			case ExpressionKind::Comma: return adlStructOf(*argument.right);
			case ExpressionKind::Conditional: {
				const StructDecl* first = adlStructOf(*argument.arguments.at(0));
				return first == adlStructOf(*argument.arguments.at(1)) ? first : nullptr;
			}
			default: return structOf(argument);
		}
	}

	std::vector<std::string> Emitter::namesOfCall(const Expression& call, std::set<std::string>* adlOnly) {
		std::vector<std::string> names = call.left->overloadNames;
		if (names.empty()) {
			names.push_back(call.left->name);
		}
		if (!call.left->unqualifiedCall) {
			return names;
		}
		const size_t tail = call.left->name.rfind("::");
		const std::string last = tail == std::string::npos ? call.left->name : call.left->name.substr(tail + 2);
		for (const ExpressionPtr& argument: call.arguments) {
			const StructDecl* decl = adlStructOf(*argument);
			std::string type = decl ? decl->name : std::string();
			if (!decl && argument->kind == ExpressionKind::Call && callsDependentTemplate(*argument)) {
				// Its type is known only once deduced: that matters only if a function of this name is in a namespace.
				const bool elsewhere = std::any_of(_unit.helpers.begin(), _unit.helpers.end(), [&](const FunctionDecl& helper) {
						return helper.name.size() > last.size() + 2
							&& helper.name.compare(helper.name.size() - last.size() - 2, std::string::npos, "::" + last) == 0;
					}) || std::any_of(_unit.templates.begin(), _unit.templates.end(), [&](const FunctionTemplate& declaration) {
						return declaration.name.size() > last.size() + 2
							&& declaration.name.compare(declaration.name.size() - last.size() - 2, std::string::npos, "::" + last) == 0;
					});
				if (elsewhere) {
					throw CompileError("an unqualified call has an argument that is a call to a function template "
						"whose result type is known only once it is deduced, and argument-dependent lookup for \""
						+ last + "\" would search its namespace; assign that argument to a variable first");
				}
				continue;
			}
			if (!decl) {
				const auto tag = enumOf(*argument);
				if (argument->kind == ExpressionKind::Conditional && !tag) {
					throw CompileError("an unqualified call has a conditional expression as an argument, whose "
						"namespace argument-dependent lookup would search; that is not lowered yet");
				}
				type = tag && *tag != "<anonymous>" ? *tag : std::string();
			}
			if (type.empty()) {
				continue;
			}
			// A type of the file scope associates the file scope: its f is found from inside a namespace too.
			// A namespace that is inline associates its enclosing one, and a namespace its inline members.
			const size_t scope = type.rfind("::");
			std::vector<std::string> spaces { scope == std::string::npos ? std::string() : type.substr(0, scope) };
			for (size_t k = 0; k < spaces.size(); ++k) {
				const std::string current = spaces[k];
				const auto add = [&](const std::string& space) {
					if (std::find(spaces.begin(), spaces.end(), space) == spaces.end()) spaces.push_back(space);
				};
				if (_unit.inlineNamespaces.count(current)) {
					const size_t outer = current.rfind("::");
					add(outer == std::string::npos ? std::string() : current.substr(0, outer));
				}
				for (const std::string& inner: _unit.inlineNamespaces) {
					const size_t outer = inner.rfind("::");
					if ((outer == std::string::npos ? std::string() : inner.substr(0, outer)) == current) add(inner);
				}
			}
			for (const std::string& space: spaces) {
				const std::string associated = space.empty() ? last : space + "::" + last;
				const std::vector<HelperFunction*> found = helpersNamed(associated);
				const std::vector<const FunctionTemplate*> foundTemplates = templatesNamed(associated);
				// An instance is looked up again where clang instantiates it, at the end of the file.
				const bool anywhere = isInstance(_helper);
				const bool visible = anywhere ? !(found.empty() && foundTemplates.empty())
					: std::any_of(found.begin(), found.end(), [&](const HelperFunction* entry) {
						return entry->firstOrder <= callerOrder();
					}) || std::any_of(foundTemplates.begin(), foundTemplates.end(), [&](const FunctionTemplate* declaration) {
						return declaration->order <= callerOrder();
					});
				if (visible && adlOnly && anywhere) {
					adlOnly->insert(associated);
				}
				if (visible && std::find(names.begin(), names.end(), associated) == names.end()) {
					// The library's own overloads compete with it, which mslc does not model here.
					// (A struct argument matches no library overload, so there is nothing to compete with.)
					if (!decl && (isMSLBuiltinName(last) || findMathBuiltin(last))) {
						throw CompileError("argument-dependent lookup finds \"" + associated + "\", which competes with the "
							"standard library's \"" + last + "\"; Apple resolves the two by overload, which mslc "
							"does not model");
					}
					names.push_back(associated);
				}
			}
		}
		return names;
	}

	std::vector<Emitter::HelperFunction*> Emitter::helpersCalled(const Expression& call) {
		std::vector<HelperFunction*> all = helpersNamed(call.left->name);
		for (const std::string& name: call.left->overloadNames) {
			if (name != call.left->name) {
				const std::vector<HelperFunction*> named = helpersNamed(name);
				all.insert(all.end(), named.begin(), named.end());
			}
		}
		return all;
	}

	// Every function a name can denote: its one function, or each signature of an overloaded name.
	std::vector<Emitter::HelperFunction*> Emitter::helpersNamed(const std::string& name) {
		if (const auto overloads = _overloads.find(name); overloads != _overloads.end()) {
			return overloads->second;
		}
		const auto found = _helpers.find(name);
		return found == _helpers.end() ? std::vector<HelperFunction*>() : std::vector<HelperFunction*> { &found->second };
	}

	std::vector<const FunctionTemplate*> Emitter::memberTemplatesOf(const std::string& owner, const std::string& member) const {
		return owner.empty() ? std::vector<const FunctionTemplate*>() : templatesNamed(owner + "::" + member);
	}

	std::vector<const FunctionTemplate*> Emitter::templatesNamed(const std::string& name) const {
		const auto found = _templatesByName.find(name);
		return found == _templatesByName.end() ? std::vector<const FunctionTemplate*>() : found->second;
	}

	// The templates a call's names denote that are declared above the call.
	std::vector<const FunctionTemplate*> Emitter::visibleTemplates(const std::vector<std::string>& names,
		const std::set<std::string>& adl) const {
		std::vector<const FunctionTemplate*> visible;
		for (const std::string& name: names) {
			for (const FunctionTemplate* declaration: templatesNamed(name)) {
				if (declaration->order > callerOrder() && !(isInstance(_helper) && adl.count(name))) {
					continue;
				}
				if (_unit.findFunction(declaration->name)) {
					throw CompileError("\"" + declaration->name + "\" names both an entry point and a function template");
				}
				visible.push_back(declaration);
			}
		}
		return visible;
	}

	// What a call can denote before its arguments are known: the templates of the names the parser found.
	std::vector<const FunctionTemplate*> Emitter::templatesCalled(const Expression& call) const {
		std::vector<const FunctionTemplate*> all = templatesNamed(call.left->name);
		for (const std::string& name: call.left->overloadNames) {
			if (name != call.left->name) {
				const std::vector<const FunctionTemplate*> named = templatesNamed(name);
				all.insert(all.end(), named.begin(), named.end());
			}
		}
		return all;
	}

	// The type a call's result has before it is emitted, for one aspect (its struct, its enum): the
	// overloads of its arity have to agree. A template returning its own parameter has no struct or enum
	// here until it is deduced.
	const Type* Emitter::returningType(const Expression& call, std::string (*aspect)(const Type&)) {
		const Type* chosen = nullptr;
		const std::vector<HelperFunction*> named = helpersCalled(call);
		const std::vector<const FunctionTemplate*> templates = templatesCalled(call);
		const size_t total = named.size() + templates.size();
		const auto consider = [&](const Type& returned) {
			if (chosen) {
				if (aspect(*chosen) != aspect(returned)) {
					throw CompileError("the overloads of \"" + call.left->name + "\" differ in what they return "
						"in a way this use of the call depends on, and which one the call chooses is not known "
						"before its arguments are evaluated; assign the call to a variable first");
				}
				return;
			}
			chosen = &returned;
		};
		for (const HelperFunction* candidate: named) {
			const FunctionDecl& definition = *candidate->definition;
			if (total > 1
				&& (candidate->firstOrder > callerOrder() || definition.parameters.size() != call.arguments.size())) {
				continue;
			}
			consider(definition.returnType);
		}
		for (const FunctionTemplate* declaration: templates) {
			if (total > 1 && declaration->order > callerOrder()) {
				continue;
			}
			std::vector<TemplateArgument> arguments;
			const FunctionDecl* pattern = templatePattern(*declaration, call.left->templateArguments, arguments);
			if (!pattern || (total > 1 && pattern->parameters.size() != call.arguments.size())) {
				continue;
			}
			consider(pattern->returnType);
		}
		return chosen;
	}

	// The overload a call names, chosen by its arguments. Arity alone often decides; otherwise the
	// arguments are evaluated here, once, left to right, and handed on in prepared so the call
	// does not evaluate them again.
	Emitter::HelperFunction* Emitter::resolveOverload(const Expression& call, const std::vector<HelperFunction*>& set,
		const std::vector<const FunctionTemplate*>& templates, const std::set<std::string>& adl,
		std::vector<Id>& prepared, const Expression* receiver) {
		// As written: Apple names the function as the call spells it.
		const size_t tail = call.left->name.rfind("::");
		const std::string name = receiver ? call.left->memberName
			: call.left->unqualifiedCall && tail != std::string::npos
			? call.left->name.substr(tail + 2) : call.left->name;
		// The receiver is the first argument of the template's signature, which is not in call.arguments.
		const size_t objects = receiver ? 1 : 0;
		const auto describe = [&](const std::vector<HelperFunction*>& entries) {
			std::string list;
			for (const HelperFunction* entry: entries) {
				list += (list.empty() ? "" : ", ") + std::string(isInstance(entry->definition) ? "template " : "")
					+ entry->definition->name + "(" + parameterSignature(*entry->definition) + ")";
			}
			return list;
		};
		const auto chosen = [&](HelperFunction* entry) {
			if (const auto instance = _instanceOf.find(entry->definition); instance != _instanceOf.end()) {
				commitInstance(*instance->second);
			}
			return definedHelper(entry);
		};
		// Explicit template arguments name a template, so a function is not a candidate.
		const bool onlyTemplates = call.left->hasTemplateArguments;
		std::vector<HelperFunction*> visible;
		std::vector<HelperFunction*> candidates;
		for (HelperFunction* entry: onlyTemplates ? std::vector<HelperFunction*>() : set) {
			if (entry->firstOrder > callerOrder() && !(isInstance(_helper) && adl.count(entry->definition->name))) {
				continue;
			}
			visible.push_back(entry);
			if (entry->definition->parameters.size() == call.arguments.size() + objects) {
				candidates.push_back(entry);
			}
		}
		// The templates whose signature takes this many arguments.
		struct TemplateCandidate {
			const FunctionTemplate* declaration;
			const FunctionDecl* pattern;
			size_t given;
			std::vector<TemplateArgument> arguments;
		};
		std::vector<TemplateCandidate> patterns;
		std::string templateList;
		for (const FunctionTemplate* declaration: templates) {
			TemplateCandidate candidate { declaration, nullptr, call.left->templateArguments.size(), {} };
			candidate.pattern = templatePattern(*declaration, call.left->templateArguments, candidate.arguments);
			if (!candidate.pattern) {
				continue;
			}
			templateList += (templateList.empty() ? "" : ", ") + describeTemplate(*declaration, *candidate.pattern);
			if (candidate.pattern->parameters.size() == call.arguments.size() + objects) {
				patterns.push_back(std::move(candidate));
			}
		}
		if (visible.empty() && templates.empty() && !onlyTemplates) {
			throw CompileError("\"" + name + "\" is called before it is declared; declare it above its first use");
		}
		if (candidates.empty() && patterns.empty()) {
			throw CompileError("no matching function for call to \"" + name + "\": it is called with "
				+ std::to_string(call.arguments.size()) + " arguments; candidates: "
				+ (describe(visible) + (visible.empty() || templateList.empty() ? "" : ", ") + templateList));
		}
		if (candidates.size() == 1 && patterns.empty()) {
			return chosen(candidates.front());
		}

		// A conditional or a comma with a place for an operand may be an lvalue, in any address space; the
		// ranking below takes every one for a thread prvalue and could pick the wrong function. One made of
		// values only is a prvalue.
		const auto placeLike = [](const auto& self, const Expression& e) -> bool {
			switch (e.kind) {
				case ExpressionKind::Identifier: case ExpressionKind::Index: case ExpressionKind::Member: return true;
				case ExpressionKind::Comma: return self(self, *e.right);
				// One place arm is enough to refuse: Apple binds `c ? la : 2.0f` to a non-const reference
				// parameter (measured), so it is not a plain prvalue.
				case ExpressionKind::Conditional:
					return self(self, *e.arguments.at(0)) || self(self, *e.arguments.at(1));
				default: return false;
			}
		};
		for (const ExpressionPtr& argument: call.arguments) {
			if ((argument->kind == ExpressionKind::Conditional || argument->kind == ExpressionKind::Comma)
				&& placeLike(placeLike, *argument)) {
				throw CompileError("a conditional or comma expression as an argument chooses among the overloads of \""
					+ name + "\", which is not lowered yet (whether it is an lvalue, and in which address space, "
					"decides the overload); assign it to a variable first");
			}
		}
		std::vector<OverloadArgument> arguments;
		if (receiver) {
			OverloadArgument object;
			object.expression = receiver;
			object.value = _pendingObject;
			arguments.push_back(std::move(object));
		}
		for (const ExpressionPtr& argument: call.arguments) {
			arguments.push_back(classifyArgument(*argument));
		}
		// A template's signature before deduction still says what kind of argument each position takes.
		std::vector<const std::vector<Parameter>*> lists;
		for (const HelperFunction* entry: candidates) {
			lists.push_back(&entry->definition->parameters);
		}
		for (const TemplateCandidate& candidate: patterns) {
			lists.push_back(&candidate.pattern->parameters);
		}
		bool enumParameter = false;
		for (const std::vector<Parameter>* list: lists) {
			for (const Parameter& parameter: *list) {
				enumParameter = enumParameter || !parameter.type.enumName.empty();
				if (parameter.type.enumName == "<anonymous>") {
					throw CompileError("an overload of \"" + name + "\" takes a parameter of an anonymous enum, "
						"which cannot be told from another anonymous enum here; that is not lowered yet");
				}
			}
		}
		for (size_t i = objects; i < arguments.size(); ++i) {
			OverloadArgument& argument = arguments[i];
			if (argument.kind == OverloadArgument::Kind::Array || argument.kind == OverloadArgument::Kind::Resource
				|| argument.pointerLike) {
				continue;
			}
			const bool bound = std::any_of(lists.begin(), lists.end(), [&](const std::vector<Parameter>* list) {
				return (*list)[i].isMutableReference();
			});
			if (bound && argument.kind == OverloadArgument::Kind::Variable && !isPlainVariable(*argument.expression)) {
				throw CompileError("argument " + std::to_string(i + 1) + " of the call to \"" + name
					+ "\" is evaluated to choose among overloads and is also bound to a reference by one; "
					"name a local variable instead");
			}
			// Whether an argument is an enum decides its rank, and enumOf only knows what it can see.
			argument.tag = enumOf(*argument.expression);
			if (enumParameter && (!argument.tag || *argument.tag == "<anonymous>")) {
				throw CompileError("argument " + std::to_string(i + 1) + " of the call to \"" + name
					+ "\" is not known to be a particular enum, and an overload takes an enum; that is not "
					"lowered yet");
			}
			// (A device or constant lvalue never binds a thread reference: that candidate just drops out.)
			if (bound && argument.kind == OverloadArgument::Kind::Variable && argument.inThread
				&& !isWholeVariable(*argument.expression)) {
				throw CompileError("argument " + std::to_string(i + 1) + " of the call to \"" + name
					+ "\" is an element or member that a mutable reference could bind, which is not lowered "
					"when choosing among overloads (a vector component cannot be bound); name a local instead");
			}
			argument.value = emitExpression(*argument.expression);
			prepared.push_back(argument.value);
		}
		// Positions that are not evaluated keep their place, so prepared[i] is argument i.
		if (!prepared.empty() && prepared.size() != arguments.size() - objects) {
			prepared.clear();
			for (size_t i = objects; i < arguments.size(); ++i) {
				prepared.push_back(arguments[i].value);
			}
		}

		// A template competes through its instance; one that cannot be deduced is no candidate (as in C++).
		for (const TemplateCandidate& candidate: patterns) {
			std::vector<TemplateArgument> deduced = candidate.arguments;
			if (!deduceTemplateArguments(*candidate.declaration, *candidate.pattern, candidate.given, arguments, deduced)) {
				continue;
			}
			try {
				candidates.push_back(&templateInstance(*candidate.declaration, std::move(deduced)).helper);
			} catch (const SubstitutionFailure&) {
				// The signature with these arguments is ill-formed (an enable_if): not a candidate.
			}
		}

		std::vector<std::pair<HelperFunction*, std::vector<ConversionCost>>> viable;
		for (HelperFunction* entry: candidates) {
			std::vector<ConversionCost> costs;
			for (size_t i = 0; i < arguments.size(); ++i) {
				// Every candidate takes the one receiver, so it cannot rank them.
				costs.push_back(i < objects ? ConversionCost { kExact, -1 }
					: conversionCost(arguments[i], entry->definition->parameters[i]));
			}
			if (std::none_of(costs.begin(), costs.end(), [](const ConversionCost& cost) { return cost.rank == kNoConversion; })) {
				viable.emplace_back(entry, std::move(costs));
			}
		}
		if (viable.empty()) {
			throw CompileError("no matching function for call to \"" + name + "\"; candidates: "
				+ describe(candidates) + (templateList.empty() ? "" : "; templates: " + templateList));
		}

		// One wins when no worse on any argument and better on one, or equal and a function beside a template.
		const auto beats = [&](const auto& first, const auto& second) {
			const std::vector<ConversionCost>& a = first.second;
			const std::vector<ConversionCost>& b = second.second;
			bool better = false;
			for (size_t i = 0; i < a.size(); ++i) {
				const bool ranked = a[i].rank != b[i].rank;
				const bool qualified = !ranked && a[i].qualification >= 0 && b[i].qualification >= 0
					&& a[i].qualification != b[i].qualification;
				if (ranked ? a[i].rank > b[i].rank : qualified && a[i].qualification > b[i].qualification) {
					return false;
				}
				better = better || (ranked ? a[i].rank < b[i].rank : qualified);
			}
			return better || (!isInstance(first.first->definition) && isInstance(second.first->definition));
		};
		for (const auto& winner: viable) {
			if (std::all_of(viable.begin(), viable.end(), [&](const auto& other) {
				return other.first == winner.first || beats(winner, other);
			})) {
				return chosen(winner.first);
			}
		}
		std::vector<HelperFunction*> tied;
		for (const auto& candidate: viable) {
			tied.push_back(candidate.first);
		}
		throw CompileError("call to \"" + name + "\" is ambiguous; candidates: " + describe(tied));
	}

	// The type a template parameter stands for while its signature is read to deduce it.
	static std::string templateMarker(const TemplateParameter& parameter) {
		return "<" + parameter.name + ">";
	}

	static bool isTemplateMarker(const Type& type) {
		return type.namedType.size() > 2 && type.namedType.front() == '<' && type.namedType.back() == '>';
	}

	// The argument as a name that tells instances apart.
	static std::string templateArgumentName(const TemplateArgument& argument) {
		if (argument.unbound) {
			return "?";
		}
		if (!argument.isType) {
			return std::to_string(argument.value);
		}
		Type bare = argument.type;
		bare.addressSpace = AddressSpace::None;
		return typeName(bare);
	}

	// Whether a constant fits the parameter's type (C++ rejects a narrowing one).
	static bool fitsTemplateConstant(ScalarKind kind, int64_t value) {
		switch (kind) {
			case ScalarKind::Bool: return value == 0 || value == 1;
			case ScalarKind::Char: return value >= -128 && value <= 127;
			case ScalarKind::UChar: return value >= 0 && value <= 255;
			case ScalarKind::Short: return value >= -32768 && value <= 32767;
			case ScalarKind::UShort: return value >= 0 && value <= 65535;
			case ScalarKind::Int: return value >= INT32_MIN && value <= INT32_MAX;
			case ScalarKind::UInt: return value >= 0 && value <= static_cast<int64_t>(UINT32_MAX);
			case ScalarKind::ULong: return value >= 0;
			default: return true;
		}
	}

	static bool sameValueType(const Type& a, const Type& b) {
		return a.scalar == b.scalar && a.vectorWidth == b.vectorWidth && a.matrixColumns == b.matrixColumns
			&& a.isPacked == b.isPacked && a.namedType == b.namedType && a.enumName == b.enumName
			&& a.isAtomic == b.isAtomic;
	}

	// A template's signature read with the explicit arguments, the type parameters to deduce standing
	// for themselves (a marker type) and a constant neither given nor defaulted unknown. Null when the
	// explicit arguments do not fit, which makes it no candidate.
	const FunctionDecl* Emitter::templatePattern(const FunctionTemplate& declaration,
		const std::vector<TemplateArgument>& given, std::vector<TemplateArgument>& arguments) {
		arguments.clear();
		if (given.size() > declaration.parameters.size()) {
			return nullptr;
		}
		for (size_t i = 0; i < declaration.parameters.size(); ++i) {
			const TemplateParameter& parameter = declaration.parameters[i];
			TemplateArgument argument;
			if (i < given.size()) {
				argument = given[i];
				if (argument.isType != parameter.isType
					|| (!parameter.isType && !fitsTemplateConstant(parameter.kind, argument.value))) {
					return nullptr;
				}
			} else if (parameter.isType) {
				argument.type.namedType = templateMarker(parameter);
			} else if (parameter.hasDefault) {
				try {
					argument = templateDefault(declaration, i, arguments);
				} catch (const SubstitutionFailure&) {
					return nullptr;
				}
				if (!fitsTemplateConstant(parameter.kind, argument.value)) {
					return nullptr;
				}
			} else {
				argument.isType = false;
				argument.unbound = true;
			}
			arguments.push_back(std::move(argument));
		}

		std::string key = std::to_string(&declaration - _unit.templates.data()) + "<";
		for (const TemplateArgument& argument: arguments) {
			key += templateArgumentName(argument) + ",";
		}
		std::unique_ptr<FunctionDecl>& slot = _patterns[key];
		if (!slot) {
			try {
				slot = std::make_unique<FunctionDecl>(instantiateTemplate(declaration, arguments, false));
			} catch (const SubstitutionFailure&) {
				_patterns.erase(key);
				return nullptr;
			}
		}
		return slot.get();
	}

	// Fills the type parameters left, from the argument types: T takes the argument's type by value or
	// reference, and T* a local array's element type; T used twice takes one type or the template is no
	// candidate. What C++ deduces and mslc does not model is an error, never a template dropped, which
	// could let another function win.
	bool Emitter::deduceTemplateArguments(const FunctionTemplate& declaration, const FunctionDecl& pattern,
		size_t given, const std::vector<OverloadArgument>& arguments, std::vector<TemplateArgument>& deduced) {
		const auto parameterIndex = [&](const Type& type) {
			for (size_t j = given; j < declaration.parameters.size(); ++j) {
				if (declaration.parameters[j].isType && isTemplateMarker(type)
					&& type.namedType == templateMarker(declaration.parameters[j])) {
					return j;
				}
			}
			return declaration.parameters.size();
		};
		const auto refuse = [&](size_t position, const std::string& why) {
			return CompileError("the type parameter of \"" + declaration.name + "\" is deduced from argument "
				+ std::to_string(position + 1) + ", " + why + "; that is not lowered yet");
		};
		std::vector<bool> known(deduced.size(), false);
		for (size_t i = 0; i < pattern.parameters.size(); ++i) {
			const Parameter& parameter = pattern.parameters[i];
			const size_t index = parameterIndex(parameter.type);
			if (index == declaration.parameters.size()) {
				continue;
			}
			const OverloadArgument& argument = arguments[i];
			Type value;
			if (parameter.type.isPointer) {
				if (argument.kind != OverloadArgument::Kind::Array) {
					if (argument.pointerLike || argument.kind == OverloadArgument::Kind::Resource) {
						throw refuse(i, "which is a pointer, a buffer or a texture and not a local array");
					}
					return false;
				}
				value = argument.binding->pointeeMsl;
				value.dropOuterDimension();
				if (value.arrayLength || value.isPointer || value.resource != ResourceKind::None) {
					throw refuse(i, "which is an array of arrays");
				}
				value.addressSpace = AddressSpace::None;
				value.isConst = argument.binding->readOnly && !parameter.type.isConst;
			} else {
				if (parameter.type.arrayLength || parameter.type.resource != ResourceKind::None) {
					throw refuse(i, "to a parameter that is an array or a texture");
				}
				if (argument.kind == OverloadArgument::Kind::Array || argument.kind == OverloadArgument::Kind::Resource
					|| argument.pointerLike) {
					throw refuse(i, "which is an array, a pointer or a texture");
				}
				if (argument.tag && !argument.tag->empty()) {
					throw refuse(i, "which is an enum");
				}
				// Apple deduces T = "device float" for such an lvalue and binds it, as a plain float& cannot.
				if (parameter.isReference && argument.kind == OverloadArgument::Kind::Variable && !argument.inThread) {
					throw refuse(i, "which is an lvalue in device, constant or threadgroup memory, where Apple "
						"deduces an address-space-qualified type");
				}
				value = deduceAutoType(Type(), _builder.typeOf(argument.value), "argument " + std::to_string(i + 1));
				// A mutable reference to a const object has T = "const float".
				value.isConst = parameter.isMutableReference() && argument.kind == OverloadArgument::Kind::Variable
					&& argument.inThread && !argument.changeable;
			}
			if (known[index]) {
				if (!sameValueType(deduced[index].type, value) || deduced[index].type.isConst != value.isConst) {
					return false;
				}
				continue;
			}
			known[index] = true;
			deduced[index].type = value;
		}

		for (size_t j = given; j < declaration.parameters.size(); ++j) {
			const TemplateParameter& parameter = declaration.parameters[j];
			if (parameter.isType && !known[j]) {
				if (!parameter.hasDefault) {
					return false;
				}
				try {
					deduced[j] = templateDefault(declaration, j, deduced);
				} catch (const SubstitutionFailure&) {
					return false;
				}
			} else if (!parameter.isType && deduced[j].unbound) {
				return false;
			}
		}
		return true;
	}

	// The instance for complete arguments, signature read and body not: one per argument list.
	Emitter::TemplateInstance& Emitter::templateInstance(const FunctionTemplate& declaration,
		std::vector<TemplateArgument> arguments) {
		std::string key = std::to_string(&declaration - _unit.templates.data()) + "<";
		for (const TemplateArgument& argument: arguments) {
			key += templateArgumentName(argument) + ",";
		}
		std::unique_ptr<TemplateInstance>& slot = _instances[key];
		if (slot) {
			return *slot;
		}
		FunctionDecl signature = instantiateTemplate(declaration, arguments, false);
		slot = std::make_unique<TemplateInstance>();
		slot->declaration = &declaration;
		slot->arguments = std::move(arguments);
		slot->decl = std::move(signature);
		slot->helper.definition = &slot->decl;
		slot->helper.firstOrder = declaration.order;
		_instanceOf[&slot->decl] = slot.get();
		return *slot;
	}

	// The instance is the function a call chose: its body is read now.
	void Emitter::commitInstance(TemplateInstance& instance) {
		if (instance.committed) {
			return;
		}
		const FunctionTemplate& declaration = *instance.declaration;
		std::string spelled = declaration.name + "<";
		for (size_t i = 0; i < instance.arguments.size(); ++i) {
			spelled += (i ? ", " : "") + templateArgumentName(instance.arguments[i]);
		}
		spelled += ">";
		if (!declaration.hasBody) {
			throw CompileError("the function template \"" + declaration.name + "\" is declared and never defined");
		}
		const auto caller = _instanceOf.find(_helper);
		const size_t depth = caller == _instanceOf.end() ? 1 : caller->second->depth + 1;
		if (depth > kMaxInstantiationDepth) {
			throw CompileError("template instantiation depth exceeds " + std::to_string(kMaxInstantiationDepth)
				+ " (instantiating " + spelled + "); Apple's limit is 1024");
		}
		instance.decl = instantiateTemplate(declaration, instance.arguments, true);
		instance.depth = depth;
		instance.committed = true;

		// The scans for file-scope samplers and imports ran before any instance existed.
		ScopeStack scopes(1);
		for (const Parameter& parameter: instance.decl.parameters) {
			scopes.back().insert(parameter.name);
		}
		std::set<std::string> names;
		collectUnshadowed(instance.decl.body.get(), scopes, names);
		for (size_t i = 0; i < instance.decl.globalsBefore && i < _unit.globals.size(); ++i) {
			if (_unit.globals[i].sampler && names.count(_unit.globals[i].name)) {
				throw CompileError("the instance " + spelled + " names the file-scope sampler \""
					+ _unit.globals[i].name + "\", which an instance does not reach; pass the sampler to it as an argument");
			}
		}
		if (_options.linkageObject) {
			std::vector<const Expression*> calls;
			collectCalls(*instance.decl.body, calls);
			for (const Expression* call: calls) {
				if (call->left->kind != ExpressionKind::Identifier) {
					continue;
				}
				for (const HelperFunction* called: helpersCalled(*call)) {
					if (!called->definition->body) {
						throw CompileError("the instance " + spelled + " calls \"" + called->definition->name
							+ "\", which this library only declares; importing from an instance of a template is "
							"not lowered");
					}
				}
			}
		}
	}

	// Whether an expression, written in a template, depends on its parameters: it names a parameter whose
	// declared type does (not a local of that name). An explicit conversion names a concrete type here, so
	// it is not, which only costs a refusal where the template wrote T(...); a call is when an argument is.
	static bool dependsOnParameter(const Expression& expression) {
		if (expression.kind == ExpressionKind::Identifier) {
			return expression.templateDependent;
		}
		if (expression.kind == ExpressionKind::Construct || expression.namesConcreteStruct) {
			return false;
		}
		const auto any = [](const ExpressionPtr& child) { return child && dependsOnParameter(*child); };
		return any(expression.left) || any(expression.right)
			|| std::any_of(expression.arguments.begin(), expression.arguments.end(), any)
			|| std::any_of(expression.elements.begin(), expression.elements.end(),
				[](const InitializerElement& element) { return dependsOnParameter(*element.value); });
	}

	bool Emitter::dependsOnTemplate(const Expression& call) {
		return std::any_of(call.arguments.begin(), call.arguments.end(),
			[](const ExpressionPtr& argument) { return dependsOnParameter(*argument); });
	}

	// Whether a call may be to a template that returns one of its own parameters.
	bool Emitter::callsDependentTemplate(const Expression& call) {
		if (call.left->kind != ExpressionKind::Identifier) {
			return false;
		}
		for (const FunctionTemplate* declaration: templatesCalled(call)) {
			std::vector<TemplateArgument> arguments;
			const FunctionDecl* pattern = declaration->order > callerOrder() ? nullptr
				: templatePattern(*declaration, call.left->templateArguments, arguments);
			if (pattern && isTemplateMarker(pattern->returnType)) {
				return true;
			}
		}
		return false;
	}

	std::string Emitter::describeTemplate(const FunctionTemplate& declaration, const FunctionDecl& pattern) const {
		return "template " + declaration.name + "(" + parameterSignature(pattern) + ")";
	}

	// A name, or an element of a local array of scalars: what a reference binds without question.
	bool Emitter::isWholeVariable(const Expression& expression) const {
		const Expression* root = &expression;
		while (root->kind == ExpressionKind::Index) {
			root = root->left.get();
		}
		if (root->kind != ExpressionKind::Identifier) {
			return false;
		}
		if (root == &expression) {
			return true;
		}
		const auto binding = _bindings.find(root->name);
		return binding != _bindings.end() && binding->second.pointeeMsl.arrayLength
			&& binding->second.pointeeMsl.vectorWidth <= 1 && binding->second.pointeeMsl.matrixColumns == 0
			&& binding->second.pointeeMsl.namedType.empty();
	}

	// A name, an element or a member of one, with nothing in it that evaluating twice would repeat.
	bool Emitter::isPlainVariable(const Expression& expression) const {
		switch (expression.kind) {
			case ExpressionKind::Identifier: return true;
			case ExpressionKind::Member: return isPlainVariable(*expression.left);
			case ExpressionKind::Index:
				return isPlainVariable(*expression.left) && expression.arguments.size() == 1
					&& (expression.arguments[0]->kind == ExpressionKind::IntLiteral
						|| expression.arguments[0]->kind == ExpressionKind::Identifier);
			default: return false;
		}
	}

	Emitter::OverloadArgument Emitter::classifyArgument(const Expression& expression) {
		OverloadArgument argument;
		argument.expression = &expression;
		const bool place = expression.kind == ExpressionKind::Identifier || expression.kind == ExpressionKind::Index
			|| expression.kind == ExpressionKind::Member;
		argument.kind = place ? OverloadArgument::Kind::Variable : OverloadArgument::Kind::Value;
		if (expression.kind == ExpressionKind::Identifier) {
			if (const Binding* binding = findResourceBinding(expression.name)) {
				const bool array = binding->isPointer && binding->pointeeMsl.arrayLength
					&& binding->storageClass == spirv::StorageClass::Function && !binding->isAddress;
				if (binding->pointeeMsl.resource != ResourceKind::None) {
					argument.kind = OverloadArgument::Kind::Resource;
				} else if (array) {
					argument.kind = OverloadArgument::Kind::Array;
				}
				argument.binding = binding;
				argument.pointerLike = argument.kind == OverloadArgument::Kind::Variable
					&& (binding->pointeeMsl.isPointer || binding->pointeeMsl.arrayLength || binding->isBuffer);
			}
		} else if (expression.kind == ExpressionKind::Unary && expression.unaryOperator == UnaryOperator::AddressOf) {
			argument.pointerLike = true;
		}
		if (argument.kind == OverloadArgument::Kind::Variable) {
			const Expression& root = storeRoot(expression);
			const Binding* local = root.kind == ExpressionKind::Identifier ? findResourceBinding(root.name) : nullptr;
			argument.inConstant = inConstantSpace(expression);
			argument.inThread = local && !local->isBuffer && local->bufferPointeeType == InvalidId
				&& local->storageClass == spirv::StorageClass::Function && !argument.inConstant;
			argument.changeable = argument.inThread && !local->readOnly;
		}
		return argument;
	}

	// How an argument of one type reaches a parameter, per the ranks above. A vector, a matrix or a
	// struct converts to nothing else; an enum is an int that converts to no enum but its own.
	Emitter::ConversionCost Emitter::conversionCost(const OverloadArgument& argument, const Parameter& parameter) {
		const Type& type = parameter.type;
		const auto exact = [](int qualification) { return ConversionCost { kExact, qualification }; };
		const ConversionCost none;

		if (type.resource != ResourceKind::None) {
			const Binding* binding = argument.kind == OverloadArgument::Kind::Resource ? argument.binding : nullptr;
			return binding && sameResourceType(binding->pointeeMsl, type) ? exact(-1) : none;
		}
		if (type.isPointer || type.arrayLength) {
			if (argument.kind != OverloadArgument::Kind::Array) {
				if (argument.expression->kind == ExpressionKind::IntLiteral && argument.expression->intValue == 0) {
					throw CompileError("a literal 0 chooses among overloads one of which takes a pointer; the "
						"null pointer conversion is not lowered yet");
				}
				if (argument.pointerLike) {
					throw CompileError("a pointer argument other than a local array (&x, a + n, a pointer or a "
						"buffer) chooses among overloads, which is not lowered yet");
				}
				return none;
			}
			return !pointerFit(*argument.binding, type).matches || (argument.binding->readOnly && !type.isConst)
				? none : exact(type.isConst ? 1 : 0);
		}
		if (argument.kind == OverloadArgument::Kind::Array || argument.kind == OverloadArgument::Kind::Resource
			|| argument.pointerLike) {
			if (declaredTypeOf(type) == _boolType) {
				throw CompileError("a pointer or array argument chooses among overloads one of which takes a bool; "
					"the pointer to bool conversion is not lowered yet");
			}
			return none;
		}

		const Id to = declaredTypeOf(type);
		const Id from = _builder.typeOf(argument.value);
		const bool enumArgument = argument.tag && !argument.tag->empty();
		if (parameter.isReference && type.addressSpace == AddressSpace::Constant) {
			// It binds a constant-space place of exactly its type, and nothing else (no conversion
			// makes a temporary in another address space).
			if (argument.kind == OverloadArgument::Kind::Variable && argument.inConstant
				&& isVectorComponent(*argument.expression)) {
				// Apple finds the candidate viable and then errors if it wins; dropping it here could
				// silently pick another overload.
				throw CompileError("an argument that is a component of a constant vector chooses among overloads "
					"one of which takes a constant reference; Apple binds it and errors, which is not modelled");
			}
			return argument.kind == OverloadArgument::Kind::Variable && argument.inConstant && from == to
				&& type.enumName == (argument.tag ? *argument.tag : std::string()) ? exact(1) : none;
		}
		if (enumArgument && from != _intType) {
			throw CompileError("an enum whose underlying type is not int chooses among overloads, which is not "
				"lowered yet (it promotes to its own type)");
		}
		if (parameter.isMutableReference()) {
			// A reference names its argument as it is: same type, a non-const local of this function.
			return argument.changeable && from == to
				&& type.enumName == (argument.tag ? *argument.tag : std::string()) ? exact(0) : none;
		}
		const int qualification = parameter.isReference ? 1 : -1;
		// A reference to thread memory cannot bind a variable of another address space.
		if (parameter.isReference && argument.kind == OverloadArgument::Kind::Variable && !argument.inThread
			&& from == to && type.enumName == (argument.tag ? *argument.tag : std::string())) {
			return none;
		}
		if (!type.enumName.empty()) {
			return enumArgument && *argument.tag == type.enumName ? exact(qualification) : none;
		}
		if (!enumArgument && from == to) {
			return exact(qualification);
		}
		// A struct read from a buffer has the buffer's layout, a type of its own: same-named is the same struct.
		if (!type.namedType.empty()) {
			const std::string* named = _types.structNameOf(from, nullptr);
			return named && *named == type.namedType
				&& !(parameter.isReference && argument.kind == OverloadArgument::Kind::Variable && !argument.inThread)
				? exact(qualification) : none;
		}
		const std::optional<ScalarKind> fromKind = _types.scalarKindOf(from);
		if (!fromKind) {
			return none;
		}
		if (_types.vectorWidth(to) > 1) {
			return _types.scalarKindOf(_types.componentOf(to)) ? ConversionCost { kSplat, -1 } : none;
		}
		const std::optional<ScalarKind> toKind = _types.scalarKindOf(to);
		if (!toKind) {
			return none;
		}
		const bool promotes = enumArgument || *fromKind == ScalarKind::Bool || *fromKind == ScalarKind::Char
			|| *fromKind == ScalarKind::UChar || *fromKind == ScalarKind::Short || *fromKind == ScalarKind::UShort;
		return { promotes && *toKind == ScalarKind::Int ? kPromotion : kConversion, -1 };
	}

	// An array parameter's own length is not part of its type (it decays), but the
	// lengths inside it are: float m[2][3] takes an array of rows of three.
	Emitter::PointerFit Emitter::pointerFit(const Binding& array, const Type& parameter) {
		PointerFit fit;
		fit.element = array.pointeeMsl;
		fit.element.dropOuterDimension();
		fit.shape = fit.element.innerArrayLengths;
		if (fit.element.arrayLength) {
			fit.shape.insert(fit.shape.begin(), *fit.element.arrayLength);
		}
		fit.element.arrayLength.reset();
		fit.element.innerArrayLengths.clear();
		fit.wanted = parameter;
		fit.wanted.isPointer = false;
		fit.wantedShape = fit.wanted.innerArrayLengths;
		fit.wanted.innerArrayLengths.clear();
		fit.matches = declaredTypeOf(fit.element) == declaredTypeOf(fit.wanted)
			&& fit.element.namedType == fit.wanted.namedType && fit.shape == fit.wantedShape;
		return fit;
	}

	// A call is an edge of the call graph. validateHelpers sees the edges whose callee a name
	// identifies; the others are checked here, against the edges emitted so far.
	void Emitter::noteCall(const FunctionDecl& callee) {
		_functionCallees[_helper ? _helper : _entryPoint].insert(&callee);
		if (!_helper) {
			return;
		}
		std::vector<const FunctionDecl*> pending { &callee };
		std::set<const FunctionDecl*> seen;
		while (!pending.empty()) {
			const FunctionDecl* at = pending.back();
			pending.pop_back();
			if (at == _helper) {
				throw CompileError("recursive call: \"" + _helper->name + "(" + parameterSignature(*_helper)
					+ ")\" reaches itself through \"" + callee.name + "(" + parameterSignature(callee)
					+ ")\"; SPIR-V for Vulkan forbids recursion (Apple's compiler accepts it)");
			}
			if (seen.insert(at).second) {
				const auto edges = _callEdges.find(at);
				if (edges != _callEdges.end()) {
					pending.insert(pending.end(), edges->second.begin(), edges->second.end());
				}
			}
		}
		_callEdges[_helper].insert(&callee);
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
		if (type.isStorageTexture()) {
			return _types.storageImage(type.resource, type.textureAccess, textureSampledKind(type.scalar));
		}
		return type.isTexture() ? _types.image(type.resource, textureSampledKind(type.scalar), type.isDepth, type.isMultisampled) : _types.samplerType();
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
		if (!binding || !sameResourceType(binding->pointeeMsl, wanted)) {
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

	// The array a pointer parameter is bound to inside the helper: the caller's array of
	// the given length, of elements the parameter points at.
	static Type boundArray(const Type& parameter, uint32_t length) {
		Type array = parameter;
		array.isPointer = false;
		array.addressSpace = AddressSpace::None;
		array.arrayLength = length;
		return array;
	}

	// The id the function is called by in the scope being emitted, with its type
	// declared and its body queued the first time. A function this unit only
	// declares is one id for the whole module, so it is declared once.
	Id Emitter::helperFunctionId(HelperFunction& helper) {
		const FunctionDecl& definition = *helper.definition;
		if (helper.type == InvalidId) {
			helper.returnType = returnsVoid(definition) ? _voidType : declaredTypeOf(definition.returnType);
			std::vector<Id> signature { helper.returnType };
			for (size_t i = 0; i < definition.parameters.size(); ++i) {
				const Parameter& parameter = definition.parameters[i];
				if (parameter.type.isPointer
					&& (parameter.type.addressSpace == AddressSpace::Device
						|| parameter.type.addressSpace == AddressSpace::Constant)) {
					helper.parameterTypes.push_back(
						_types.bufferPointer(bufferParamTypes(parameter.type).second));
				} else {
					helper.parameterTypes.push_back(parameter.type.isPointer
						? _types.pointer(spirv::StorageClass::Function,
							arrayTypeOf(boundArray(parameter.type, helper.arrayLengths[i])))
						: parameter.isMutableReference()
						? _types.pointer(spirv::StorageClass::Function, declaredTypeOf(parameter.type))
						: helperParameterType(parameter.type));
				}
				signature.push_back(helper.parameterTypes.back());
			}

			// One OpTypeFunction per distinct signature: a duplicate fails validation.
			const auto slot = _functionTypes.emplace(signature, InvalidId);
			if (slot.second) {
				slot.first->second = _builder.emitDecl(spirv::OpTypeFunction, signature);
			}

			helper.type = slot.first->second;
		}

		const auto slot = helper.ids.emplace(helperScope(helper), InvalidId);
		if (slot.second) {
			slot.first->second = _builder.nextId();
			_helperQueue.push_back(&helper);
		}

		return slot.first->second;
	}

	// A helper with a body is emitted once per stage that reaches it, or once per entry point
	// when it names a file-scope sampler, which is that entry point's own variable. One the unit
	// only declares, and one a library exports, belong to no stage.
	std::pair<Stage, const FunctionDecl*> Emitter::helperScope(const HelperFunction& helper) const {
		if (!helper.definition->body) {
			return { Stage::None, nullptr };
		}
		return { _entryPoint->stage, _samplerDependent.count(helper.definition) ? _entryPoint : nullptr };
	}

	// Arguments are evaluated left to right and converted to the parameter types.
	// Resource arguments retain their caller's identity, including writes.
	Id Emitter::emitHelperCall(const Expression& call, HelperFunction& generic, const Expression* receiver,
		const std::vector<Id>& prepared) {
		const FunctionDecl& definition = *generic.definition;
		noteCall(definition);
		// The scan before emission does not follow argument-dependent lookup or template instances, so a
		// caller it did not mark would share one body between entry points and call the first one's sampler.
		if (_helper && _samplerDependent.count(&definition) && !_samplerDependent.count(_helper)) {
			throw CompileError("\"" + _helper->name + "\" reaches \"" + definition.name + "\", which names a "
				"file-scope sampler, only through argument-dependent lookup or a template instance; pass the sampler "
				"as a parameter or call it by a qualified name");
		}
		std::vector<Id> arrays(definition.parameters.size(), InvalidId);
		HelperFunction& helper = pointerSpecialisation(call, generic, receiver ? 1 : 0, arrays);

		std::vector<uint32_t> operands { helperFunctionId(helper) };
		std::vector<std::pair<Id, Id>> copyOuts;
		std::vector<std::optional<StdlibOperand>> argumentOperands(call.arguments.size());
		const auto stdlibArgumentOperand = [&](Id type) -> std::optional<StdlibOperand> {
			const uint32_t width = _types.vectorWidth(type);
			if (const auto scalar = _types.scalarKindOf(width > 1 ? _types.componentOf(type) : type)) {
				return StdlibOperand{*scalar, width};
			}
			return std::nullopt;
		};
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

			if (parameter.type.isPointer) {
				operands.push_back(arrays[first + i]);
				continue;
			}

			if (parameter.isReference && parameter.type.addressSpace == AddressSpace::Constant) {
				const Id value = constantReferenceArgument(*call.arguments[i], parameter, definition.name, i + 1,
					prepared.empty() ? InvalidId : prepared[i]);
				if (parameter.type.enumName.empty()) {
					argumentOperands[i] = stdlibArgumentOperand(_builder.typeOf(value));
				}
				operands.push_back(convertImplicit(value, helper.parameterTypes[first + i]));
				continue;
			}

			if (parameter.isMutableReference()) {
				operands.push_back(referenceArgument(*call.arguments[i], parameter, definition.name, i + 1, copyOuts));
				// An enum-typed parameter is an exact match for an enum argument, which beats the
				// int promotion a library signature would need; leaving no operand reads as no
				// match, same as any other type the library's signatures do not model.
				if (parameter.type.enumName.empty()) {
					argumentOperands[i] = stdlibArgumentOperand(valueTypeAt(operands.back()));
				}
				continue;
			}

			if (!parameter.type.enumName.empty()) {
				requireEnumValue(parameter.type.enumName, *call.arguments[i],
					"argument " + std::to_string(i + 1) + " of the call to \"" + definition.name + "\"");
			}
			const Id value = prepared.empty() ? emitExpression(*call.arguments[i]) : prepared[i];
			if (parameter.type.enumName.empty()) {
				argumentOperands[i] = stdlibArgumentOperand(_builder.typeOf(value));
			}
			try {
				operands.push_back(convertImplicit(value, helper.parameterTypes[first + i]));
			} catch (const CompileError& error) {
				throw CompileError("argument " + std::to_string(i + 1) + " of the call to \""
					+ definition.name + "\" cannot be converted to the parameter type "
					+ typeName(definition.parameters[first + i].type) + ": " + error.what());
			}
		}

		// The standard library's overloads compete with this function where its names are
		// visible beside it. Where its parameters are one of theirs, Apple never calls it
		// alone (ambiguous, or another library overload wins); where the arguments are exactly
		// one of theirs, that one wins or ties.
		if (call.left->besideStdlib) {
			const size_t tail = definition.name.rfind("::");
			const std::string stdlibName = tail == std::string::npos ? definition.name : definition.name.substr(tail + 2);
			std::vector<std::optional<StdlibOperand>> parameterOperands;
			for (const Parameter& parameter: definition.parameters) {
				parameterOperands.push_back(stdlibOperand(parameter));
			}
			if (isStdlibSignature(stdlibName, parameterOperands) || isStdlibSignature(stdlibName, argumentOperands)) {
				throw CompileError("the call to \"" + definition.name + "\" also matches the standard library's \""
					+ stdlibName + "\", which is visible beside it; Apple resolves the two by overload, which mslc "
					"does not model");
			}
		}

		const Id result = _builder.emitTyped(spirv::OpFunctionCall, helper.returnType, operands);
		for (const auto& [place, temporary]: copyOuts) {
			const Id type = valueTypeAt(place);
			_builder.emit(spirv::OpStore, { place, loadFrom(temporary, type) });
		}
		return result;
	}

	// A device or constant pointer argument names a whole buffer: an entry point's
	// own buffer parameter, or the pointer parameter of the calling helper. Apple
	// also takes &b[i] and b + n, pointers into the middle of one, and pointer
	// values copied or compared; none of those is a name the helper's parameter
	// can hold, so each is refused.
	Id Emitter::bufferPointerArgument(const Expression& argument, const Parameter& parameter,
		const std::string& what) {
		const std::string space = addressSpaceName(parameter.type.addressSpace);
		if (argument.kind != ExpressionKind::Identifier) {
			throw CompileError(what + " has to name a whole buffer, because parameter \"" + parameter.name
				+ "\" is a pointer in the " + space + " address space, and &b[i] and other pointers "
				"into the middle of one are not lowered yet");
		}
		if (argument.fileScope) {
			// A file-scope object is a constant, a sampler or a struct, never a buffer; without
			// this a "::data" spelled past a kernel buffer of the same name would silently bind
			// the buffer, because this path never calls emitExpression's file-scope correction.
			throw CompileError(what + " is \"" + argument.name + "\", which names a file-scope object, "
				+ "and a file-scope object is not a buffer; parameter \"" + parameter.name + "\" is a "
				+ "pointer in the " + space + " address space and takes one");
		}
		const auto binding = _bindings.find(argument.name);
		if (binding == _bindings.end() || !binding->second.isPointer || !binding->second.isBuffer
			|| binding->second.bufferPointeeType == InvalidId) {
			throw CompileError(what + " is \"" + argument.name + "\", which is not a buffer; parameter \""
				+ parameter.name + "\" is a pointer in the " + space + " address space and takes one");
		}
		if (binding->second.pointeeMsl.addressSpace != parameter.type.addressSpace) {
			throw CompileError(what + " is in the "
				+ addressSpaceName(binding->second.pointeeMsl.addressSpace) + " address space, but "
				+ "parameter \"" + parameter.name + "\" takes a pointer in the " + space
				+ " one, and Metal does not convert between address spaces");
		}
		// The pointee has to be the type the parameter names, as the source spells it and
		// as it is laid out: an atomic_uint buffer and a uint one share a block struct, an
		// enum and its underlying type share the scalar, and a float3 buffer and a
		// packed_float3 one share a vector type but stride 16 and 12. Ordinary loads and
		// stores through the parameter would read or write the wrong thing, so only a
		// difference in const qualification is allowed.
		const Type& given = binding->second.pointeeMsl;
		if (given.isAtomic != parameter.type.isAtomic || given.enumName != parameter.type.enumName
			|| binding->second.bufferPointeeType != bufferParamTypes(parameter.type).second) {
			throw CompileError(what + " is a buffer of " + typeName(given) + ", and parameter \""
				+ parameter.name + "\" points at " + typeName(parameter.type)
				+ ", which is not the same element type");
		}
		// A constant-space pointer can never write, so a read-only buffer binds it;
		// only a device pointer that is not const could change a const buffer.
		const bool canWrite = parameter.type.addressSpace == AddressSpace::Device
			&& !parameter.type.isConst;
		if (binding->second.readOnly && canWrite) {
			throw CompileError(what + " is const, and parameter \"" + parameter.name
				+ "\" is a pointer that can change it");
		}
		return bufferBase(binding->second);
	}

	// A thread pointer parameter takes a local array of its element type. Vulkan
	// without VariablePointers cannot index a pointer to an element by a run-time
	// value, so the helper is emitted once per list of array lengths and its
	// parameter is a pointer to the whole array, which p[i] indexes with an access
	// chain. The helper then reads and writes the caller's own storage. Apple also
	// takes &x, a + 1 and a pointer reassigned in the body; none of those is an
	// array the helper can name, so each is refused.
	Emitter::HelperFunction& Emitter::pointerSpecialisation(const Expression& call, HelperFunction& generic,
		size_t first, std::vector<Id>& arrays) {
		const FunctionDecl& definition = *generic.definition;
		std::vector<uint32_t> lengths(definition.parameters.size(), 0);
		bool any = false;
		for (size_t i = 0; i < call.arguments.size(); ++i) {
			const Parameter& parameter = definition.parameters[first + i];
			if (!parameter.type.isPointer) {
				continue;
			}
			any = true;

			const std::string what = "argument " + std::to_string(i + 1) + " of the call to \""
				+ definition.name + "\"";
			const Expression& argument = *call.arguments[i];

			// A device or constant pointer takes a whole buffer: an entry point's
			// own buffer parameter, or the pointer parameter of the calling helper.
			if (parameter.type.addressSpace == AddressSpace::Device
				|| parameter.type.addressSpace == AddressSpace::Constant) {
				arrays[first + i] = bufferPointerArgument(argument, parameter, what);
				continue;
			}

			const auto binding = argument.kind == ExpressionKind::Identifier
				? _bindings.find(argument.name) : _bindings.end();
			if (binding == _bindings.end() || !binding->second.isPointer || !binding->second.pointeeMsl.arrayLength
				|| binding->second.storageClass != spirv::StorageClass::Function || binding->second.isAddress) {
				throw CompileError(what + " has to name a local array (or a pointer parameter of the calling "
					"helper), since parameter \"" + parameter.name + "\" is a pointer to its elements; "
					"&x, a + n and pointers into a buffer are not lowered yet");
			}

			const PointerFit fit = pointerFit(binding->second, parameter.type);
			const Type& element = fit.element;
			const Type& wanted = fit.wanted;
			const std::vector<uint32_t>& shape = fit.shape;
			const std::vector<uint32_t>& wantedShape = fit.wantedShape;
			if (!fit.matches) {
				const auto spelled = [&](const Type& base, const std::vector<uint32_t>& lengths) {
					std::string name = typeName(base);
					for (const uint32_t length: lengths) { name += "[" + std::to_string(length) + "]"; }
					return name;
				};
				throw CompileError(what + " is an array of " + spelled(element, shape) + ", and parameter \""
					+ parameter.name + "\" points at " + spelled(wanted, wantedShape));
			}
			if (binding->second.readOnly && !parameter.type.isConst) {
				throw CompileError(what + " is const, and parameter \"" + parameter.name
					+ "\" is a pointer that can change it");
			}

			lengths[first + i] = *binding->second.pointeeMsl.arrayLength;
			arrays[first + i] = binding->second.id;
		}

		if (!any) {
			return generic;
		}

		const auto slot = _pointerSpecialisations.emplace(std::make_pair(generic.definition, lengths), generic);
		if (slot.second) {
			slot.first->second.arrayLengths = lengths;
		}
		return slot.first->second;
	}

	// An element, a member, or a reference local bound to one is passed through a
	// temporary copied back after the call, so a second reference argument naming
	// the same storage would have its write overwritten by the stale copy. Two
	// bare variables are the same pointer and alias as C++ does.
	void Emitter::rejectAliasedReferenceArguments(const Expression& call, const FunctionDecl& definition,
		size_t first) {
		struct Passed { std::string root; bool temporary; std::vector<AccessStep> steps; };
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
			std::string rootName;
			std::vector<AccessStep> steps;
			accessChain(argument, rootName, steps);
			passed.push_back({ root.name, argument.kind != ExpressionKind::Identifier
				|| (bound != _bindings.end() && bound->second.isAddress), std::move(steps) });
		}

		// A copied-in element or member is stale for anything else that reads or writes the same
		// storage during the call: a pointer parameter handed the whole array, or an argument that
		// writes (assignment, a call of a helper) while naming that storage.
		const bool copiedIn = std::any_of(passed.begin(), passed.end(), [](const Passed& p) { return p.temporary; });
		if (copiedIn) {
			const auto mayWrite = [&](const auto& self, const Expression& e) -> bool {
				if (e.kind == ExpressionKind::Assign) {
					return true;
				}
				// A helper writes the caller's storage only through a reference or a pointer parameter.
				if (e.kind == ExpressionKind::Call && e.left->kind == ExpressionKind::Identifier) {
					const auto takesAddress = [](const HelperFunction& helper) {
						return std::any_of(helper.definition->parameters.begin(), helper.definition->parameters.end(),
							[](const Parameter& p) { return p.isMutableReference() || p.type.isPointer; });
					};
					const auto overloads = _overloads.find(e.left->name);
					const auto single = _helpers.find(e.left->name);
					if (overloads != _overloads.end()
						? std::any_of(overloads->second.begin(), overloads->second.end(),
							[&](const HelperFunction* h) { return takesAddress(*h); })
						: single != _helpers.end() && takesAddress(single->second)) {
						return true;
					}
				}
				for (const Expression* child: { e.left.get(), e.right.get() }) {
					if (child && self(self, *child)) return true;
				}
				return std::any_of(e.arguments.begin(), e.arguments.end(),
					[&](const ExpressionPtr& a) { return self(self, *a); });
			};
			std::set<std::string> roots;
			for (const Passed& p: passed) {
				if (p.temporary) roots.insert(p.root);
			}
			for (size_t i = 0; i < call.arguments.size(); ++i) {
				const Parameter& parameter = definition.parameters[first + i];
				if (parameter.isMutableReference()) {
					continue;
				}
				std::set<std::string> names;
				collectIdentifiers(*call.arguments[i], names);
				const bool sharesRoot = std::any_of(names.begin(), names.end(),
					[&](const std::string& n) { return roots.count(n) > 0; });
				if ((parameter.type.isPointer && sharesRoot) || mayWrite(mayWrite, *call.arguments[i])) {
					throw CompileError("the call to \"" + definition.name + "\" passes an element or member through "
						"a reference parameter, and another argument reads or writes the same storage "
						"(a pointer to it, or an expression with side effects); the reference is a copy "
						"that is written back after the call, so that is not lowered yet");
				}
			}
		}

		// Two places of one variable that no run can make the same (different members, or different
		// literal indices at the first step they differ) are separate copies in and out, so the callee
		// cannot tell them from two variables. One that is a prefix of the other, or a dynamic index,
		// may overlap and stays refused.
		const auto disjoint = [](const std::vector<AccessStep>& a, const std::vector<AccessStep>& b) {
			for (size_t k = 0; k < a.size() && k < b.size(); ++k) {
				if (a[k].isDereference || a[k].isArrow || b[k].isDereference || b[k].isArrow
					|| a[k].isIndex != b[k].isIndex) {
					return false;
				}
				if (!a[k].isIndex) {
					if (a[k].memberName != b[k].memberName) {
						return true;
					}
					continue;
				}
				const bool literals = a[k].index->kind == ExpressionKind::IntLiteral
					&& b[k].index->kind == ExpressionKind::IntLiteral;
				if (!literals) {
					return false;
				}
				if (a[k].index->intValue != b[k].index->intValue) {
					return true;
				}
			}
			return false;
		};

		for (size_t i = 0; i < passed.size(); ++i) {
			for (size_t j = i + 1; j < passed.size(); ++j) {
				const bool sameRoot = passed[i].root == passed[j].root;
				const bool viaAddress = (passed[i].temporary || passed[j].temporary)
					&& (_bindings.count(passed[i].root) && _bindings.at(passed[i].root).isAddress
						|| _bindings.count(passed[j].root) && _bindings.at(passed[j].root).isAddress);
				const bool apart = sameRoot && !viaAddress && disjoint(passed[i].steps, passed[j].steps);
				if (!apart && ((sameRoot && (passed[i].temporary || passed[j].temporary)) || viaAddress)) {
					throw CompileError("the call to \"" + definition.name + "\" passes the same variable "
						"through two reference parameters, one of them as a member or element, and the two "
						"places may overlap (one holds the other, or an index is not a literal); that is "
						"not lowered yet");
				}
			}
		}
	}

	// A struct value that is computed rather than named: S{...}, a call, or a conditional with
	// such an arm. A conditional of named arms may yield a place, so it is not one.
	static bool isComputedValue(const Expression& expression) {
		switch (expression.kind) {
			case ExpressionKind::Identifier: return false;
			case ExpressionKind::Member:
			case ExpressionKind::Index: return isComputedValue(*expression.left);
			case ExpressionKind::Comma: return isComputedValue(*expression.right);
			// One prvalue arm makes the whole conditional a prvalue.
			case ExpressionKind::Conditional:
				return isComputedValue(*expression.arguments[0]) || isComputedValue(*expression.arguments[1]);
			case ExpressionKind::Call:
			case ExpressionKind::InitList: return true;
			default: return false;
		}
	}

	// A member or element of a computed value: a prvalue, so nothing can be stored to it
	// or bound to a reference.
	static bool isTemporaryPart(const Expression& target) {
		return (target.kind == ExpressionKind::Member || target.kind == ExpressionKind::Index)
			&& isComputedValue(target);
	}

	// The messages for a place that is a member of a conditional or comma whose arms are
	// named: Apple takes them, mslc does not lower them.
	static void rejectPlaceThroughConditional(const Expression& target, const char* what) {
		if (target.kind != ExpressionKind::Member && target.kind != ExpressionKind::Index) {
			return;
		}
		const Expression* root = &target;
		while (root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) {
			root = root->left.get();
		}
		if (root->kind == ExpressionKind::Conditional) {
			throw CompileError(std::string(what) + " a member or element of a conditional expression is not "
				"supported; use the variable in an if instead");
		}
		if (root->kind == ExpressionKind::Comma) {
			throw CompileError(std::string(what) + " a member or element of a comma expression is not "
				"lowered yet; use its right operand on its own");
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
		if (bindsTemporary(argument)) {
			throw CompileError("no matching function for call to \"" + function + "\": " + what
				+ " is a temporary, which parameter \"" + parameter.name + "\" cannot bind as a reference");
		}
		if (isVectorComponent(argument)) {
			throw CompileError(what + " is a component of a vector, which a reference parameter cannot "
				"name (Apple refuses it too)");
		}
		rejectPlaceThroughConditional(argument, "passing by reference");

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

	// Whether a name, an element or a member lives in the constant address space: a constant buffer, a
	// file-scope constant, or a constant reference parameter of the function being emitted.
	bool Emitter::inConstantSpace(const Expression& place) {
		const Expression& root = storeRoot(place);
		if (const Type* pointer = pointerMemberType(root)) {
			return pointer->addressSpace == AddressSpace::Constant;
		}
		if (root.kind != ExpressionKind::Identifier) {
			return false;
		}
		const auto binding = _bindings.find(root.name);
		if (binding != _bindings.end()) {
			return binding->second.pointeeMsl.addressSpace == AddressSpace::Constant;
		}
		return _constants.count(root.name) > 0;
	}

	// The type a name, an element or a member has, as the source spelled it; a buffer pointer counts as an
	// array. Nothing for an expression that is not one of these.
	std::optional<Type> Emitter::placeType(const Expression& place) {
		switch (place.kind) {
			case ExpressionKind::Identifier: {
				// "::x" is the file-scope x, whatever local hides it.
				const auto binding = place.fileScope ? _bindings.end() : _bindings.find(place.name);
				if (binding != _bindings.end()) {
					Type type = binding->second.pointeeMsl;
					if (binding->second.isBuffer) {
						if (type.arrayLength) {
							type.innerArrayLengths.insert(type.innerArrayLengths.begin(), *type.arrayLength);
						}
						type.arrayLength = 1;
					}
					return type;
				}
				for (const VariableDeclaration& global: _unit.globals) {
					if (global.name == place.name) {
						return global.type;
					}
				}
				return std::nullopt;
			}
			case ExpressionKind::Member: {
				// A member of "::s" finds its struct from the file-scope s, not the local that hides it.
				const Expression* root = place.left.get();
				while (root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) {
					root = root->left.get();
				}
				const StructDecl* decl = nullptr;
				if (root->kind == ExpressionKind::Identifier && root->fileScope) {
					const std::optional<Type> owner = placeType(*place.left);
					decl = owner && !owner->isPointer ? _unit.findStruct(owner->namedType) : nullptr;
				} else {
					decl = structOf(*place.left);
				}
				if (!decl) {
					return std::nullopt;
				}
				std::string name;
				const size_t field = fieldIndexOf(*decl, place.memberName, name);
				return field == decl->fields.size() ? std::nullopt : std::optional<Type>(decl->fields[field].type);
			}
			case ExpressionKind::Index: {
				std::optional<Type> type = placeType(*place.left);
				if (!type) {
					return std::nullopt;
				}
				// An index of a pointer member, s.p[i] or *s.p, is its pointee.
				if (type->isPointer) {
					type->isPointer = false;
					return type;
				}
				if (type->arrayLength) {
					type->dropOuterDimension();
				} else if (type->isMatrix()) {
					type->matrixColumns = 0;
				} else if (type->vectorWidth > 1) {
					type->vectorWidth = 0;
				} else {
					return std::nullopt;
				}
				return type;
			}
			default: return std::nullopt;
		}
	}

	// The type a sizeof operand that is a name, an element or a member has, as the source spelled it:
	// a packed vector is 12 bytes where the value of one is a float3 of 16. A swizzle of a vector
	// place is an unpacked vector of its length.
	std::optional<Type> Emitter::sizeofPlaceType(const Expression& place) {
		if (place.kind == ExpressionKind::Member && !structOf(*place.left)) {
			std::optional<Type> base = sizeofPlaceType(*place.left);
			if (base && !base->namedType.empty() && !base->isPointer) {
				// structOf found the struct of a local that hides "::name"; placeType reads the file-scope one.
				return placeType(place);
			}
			const size_t lanes = place.memberName.size();
			if (!base || !base->isVector() || base->isMatrix() || base->arrayLength || base->isPointer
				|| lanes < 1 || lanes > 4) {
				return std::nullopt;
			}
			// Letters of one set, each naming a component the vector has.
			const std::string letters = place.memberName.find_first_of("xyzw") == 0 ? "xyzw" : "rgba";
			for (const char letter: place.memberName) {
				const size_t index = letters.find(letter);
				if (index == std::string::npos || index >= base->vectorWidth) {
					return std::nullopt;
				}
			}
			base->isPacked = false;
			base->vectorWidth = lanes > 1 ? static_cast<uint32_t>(lanes) : 0;
			return base;
		}
		return placeType(place);
	}

	// The type of a sizeof operand worked out from the source alone, by C's rules, without building a
	// value: nothing in the operand runs or is emitted. A packed vector keeps its packed type through
	// the unary and binary operators (Apple: sizeof is 12). Nothing for an operand whose type is not
	// certain, so the caller refuses it rather than guess.
	std::optional<Type> Emitter::sizeofExpressionType(const Expression& expression) {
		const auto plain = [](const Type& type) {
			return !type.isPointer && !type.arrayLength && !type.isMatrix() && type.namedType.empty()
				&& type.enumName.empty() && type.resource == ResourceKind::None && !type.isAtomic
				&& !type.isAuto && type.scalar != ScalarKind::Void && type.scalar != ScalarKind::Double;
		};
		const auto scalarOf = [](ScalarKind kind) { Type type; type.scalar = kind; return type; };
		// The integer promotions: anything narrower than an int, a bool included, is an int.
		const auto promoted = [&](Type type) {
			if (type.vectorWidth == 0 && !isFloatKind(type.scalar)
				&& (type.scalar == ScalarKind::Bool || promotedKind(type.scalar) != type.scalar)) {
				type.scalar = ScalarKind::Int;
			}
			return type;
		};
		// The usual arithmetic conversions of two scalar kinds: a float beats an integer and a float is
		// the wider of the two, then C's integer rules.
		const auto commonKind = [](ScalarKind left, ScalarKind right) {
			const bool leftFloat = isFloatKind(left);
			const bool rightFloat = isFloatKind(right);
			if (leftFloat || rightFloat) {
				return leftFloat && rightFloat ? (left == ScalarKind::Float || right == ScalarKind::Float
					? ScalarKind::Float : ScalarKind::Half) : leftFloat ? left : right;
			}
			const auto promoteInt = [](ScalarKind kind) {
				return kind == ScalarKind::Bool ? ScalarKind::Int : promotedKind(kind);
			};
			return commonIntegerKind(promoteInt(left), promoteInt(right));
		};
		// What a binary operator of two plain arithmetic operands gives: scalars meet in the usual
		// conversions; two vectors of one element type and width give the left one's type, packed or
		// not (Apple: packed - float3 is 12, float3 - packed is 16); a vector beside a scalar of its own
		// element type is the vector; anything else is not modelled.
		const auto arithmetic = [&](const Type& left, const Type& right) -> std::optional<Type> {
			if (left.isScalar() && right.isScalar()) {
				return scalarOf(commonKind(left.scalar, right.scalar));
			}
			if (left.scalar != right.scalar || left.scalar == ScalarKind::Bool || (left.vectorWidth != right.vectorWidth
				&& !left.isScalar() && !right.isScalar())) {
				return std::nullopt;
			}
			return left.isScalar() ? right : left;
		};

		// What a binary operator gives for operands of these types, or nothing where the operator does not
		// take them (a shift or a remainder of a float) or the result is not modelled.
		const auto binaryType = [&](BinaryOperator op, const Type& left, const Type& right) -> std::optional<Type> {
			// A pointer moved by an integer is that pointer.
			const bool offset = op == BinaryOperator::Add || op == BinaryOperator::Subtract;
			const auto integer = [&](const Type& type) { return plain(type) && type.isScalar() && !isFloatKind(type.scalar); };
			if (offset && left.isPointer && !left.arrayLength && integer(right)) {
				return left;
			}
			if (op == BinaryOperator::Add && right.isPointer && !right.arrayLength && integer(left)) {
				return right;
			}
			if (!plain(left) || !plain(right)) {
				return std::nullopt;
			}
			switch (op) {
				case BinaryOperator::Less: case BinaryOperator::LessEqual: case BinaryOperator::Greater:
				case BinaryOperator::GreaterEqual: case BinaryOperator::Equal: case BinaryOperator::NotEqual:
				case BinaryOperator::LogicalAnd: case BinaryOperator::LogicalOr:
					return left.isScalar() && right.isScalar() ? std::optional<Type>(scalarOf(ScalarKind::Bool)) : std::nullopt;
				case BinaryOperator::ShiftLeft: case BinaryOperator::ShiftRight:
					return left.isScalar() && right.isScalar() && !isFloatKind(left.scalar) && !isFloatKind(right.scalar)
						? std::optional<Type>(promoted(left)) : std::nullopt;
				case BinaryOperator::Modulo:
				case BinaryOperator::BitAnd: case BinaryOperator::BitOr: case BinaryOperator::BitXor:
					return isFloatKind(left.scalar) || isFloatKind(right.scalar) ? std::nullopt : arithmetic(left, right);
				default:
					return arithmetic(left, right);
			}
		};
		// Whether a value of type `from` converts to an object of type `to` the way Apple accepts: any
		// scalar to any scalar, otherwise the same vector, matrix, struct or pointer.
		const auto converts = [&](const Type& to, const Type& from) {
			if (to.isPointer || from.isPointer) {
				return to.isPointer && from.isPointer && !to.arrayLength && !from.arrayLength && to.scalar == from.scalar
					&& to.namedType == from.namedType && to.vectorWidth == from.vectorWidth
					&& to.matrixColumns == from.matrixColumns && to.isPacked == from.isPacked;
			}
			if (to.arrayLength || from.arrayLength || to.resource != ResourceKind::None || from.resource != ResourceKind::None
				|| to.isAuto || from.isAuto || !to.enumName.empty() || !from.enumName.empty() || to.isAtomic || from.isAtomic) {
				return false;
			}
			if (!to.namedType.empty() || !from.namedType.empty()) {
				return to.namedType == from.namedType;
			}
			if (to.isScalar() && from.isScalar() && !to.isMatrix() && !from.isMatrix()) {
				return to.scalar != ScalarKind::Void && from.scalar != ScalarKind::Void;
			}
			return to.scalar == from.scalar && to.vectorWidth == from.vectorWidth && to.matrixColumns == from.matrixColumns;
		};
		// Whether a place cannot be written: a const object, one in the constant address space, or a member of one.
		const auto readOnly = [&](const Expression& place) {
			const Expression* found = &storeRoot(place);
			if (pointerMemberType(*found)) {
				// Through the pointer its pointee decides; the pointer itself is part of its struct.
				if (found != &place) {
					return readOnlyRoot(*found);
				}
				found = &storeRoot(*place.left);
			}
			const Expression& root = *found;
			if (root.kind != ExpressionKind::Identifier) {
				return true;
			}
			const auto binding = root.fileScope ? _bindings.end() : _bindings.find(root.name);
			if (binding != _bindings.end()) {
				return binding->second.readOnly || binding->second.pointeeMsl.isConst;
			}
			return true;
		};

		switch (expression.kind) {
			case ExpressionKind::IntLiteral: return scalarOf(expression.intKind);
			case ExpressionKind::FloatLiteral: return scalarOf(expression.floatIsHalf ? ScalarKind::Half : ScalarKind::Float);
			case ExpressionKind::BoolLiteral: return scalarOf(ScalarKind::Bool);
			case ExpressionKind::Sizeof: return scalarOf(ScalarKind::ULong);
			case ExpressionKind::Construct: return expression.constructType;
			case ExpressionKind::InitList:
				if (expression.namesConcreteStruct && expression.constructType) {
					return expression.constructType;
				}
				return std::nullopt;
			case ExpressionKind::Identifier:
			case ExpressionKind::Member:
			case ExpressionKind::Index: {
				if (expression.kind == ExpressionKind::Identifier && !expression.fileScope) {
					const auto binding = _bindings.find(expression.name);
					if (binding != _bindings.end() && binding->second.isBuffer) {
						Type pointer = binding->second.pointeeMsl;
						pointer.isPointer = true;
						pointer.arrayLength.reset();
						pointer.innerArrayLengths.clear();
						return pointer;
					}
					// A helper's pointer parameter is bound like a local array, so a local that hides it
					// cannot be told apart there. An entry point's is a buffer binding, which a hiding
					// local is not.
					if (_helper && namesPointerParameter(expression.name)) {
						return std::nullopt;
					}
				}
				return sizeofPlaceType(expression);
			}
			case ExpressionKind::Conditional: {
				const std::optional<Type> condition = sizeofExpressionType(*expression.left);
				if (!condition || !plain(*condition) || !condition->isScalar()) {
					return std::nullopt;
				}
				const std::optional<Type> yes = sizeofExpressionType(*expression.arguments[0]);
				const std::optional<Type> no = sizeofExpressionType(*expression.arguments[1]);
				if (yes && no && yes->isPointer && no->isPointer && !yes->arrayLength && !no->arrayLength
					&& yes->scalar == no->scalar && yes->namedType == no->namedType && yes->vectorWidth == no->vectorWidth
					&& yes->matrixColumns == no->matrixColumns && yes->isPacked == no->isPacked
					&& yes->addressSpace == no->addressSpace) {
					return yes;
				}
				if (!yes || !no || !plain(*yes) || !plain(*no)) {
					return std::nullopt;
				}
				if (yes->scalar == no->scalar && yes->vectorWidth == no->vectorWidth) {
					return yes;
				}
				return yes->isScalar() && no->isScalar() ? std::optional<Type>(scalarOf(commonKind(yes->scalar, no->scalar)))
					: std::nullopt;
			}
			case ExpressionKind::Comma:
				// The left side is not built, but Apple still rejects one that is not an expression.
				return sizeofExpressionType(*expression.left) ? sizeofExpressionType(*expression.right) : std::nullopt;
			case ExpressionKind::Call: {
				// The return type of the one function the name can find: any overload, template, library
				// function beside it or argument-dependent lookup would need the arguments' types.
				if (expression.left->kind != ExpressionKind::Identifier || expression.left->hasTemplateArguments
					|| _bindings.count(expression.left->name) > 0
					|| (expression.left->besideStdlib && findMathBuiltin(expression.left->name))) {
					return std::nullopt;
				}
				std::set<std::string> adl;
				const std::vector<std::string> names = namesOfCall(expression, &adl);
				if (names.size() != 1 || !adl.empty() || !visibleTemplates(names, adl).empty()) {
					return std::nullopt;
				}
				const std::vector<HelperFunction*> set = helpersNamed(names.front());
				if (set.size() != 1 || set.front()->definition->parameters.size() != expression.arguments.size()) {
					return std::nullopt;
				}
				// Each argument has to be one the parameter takes: a mismatch is an error in Apple, and
				// a reference parameter binds to a place, which is not modelled.
				for (size_t i = 0; i < expression.arguments.size(); ++i) {
					const Parameter& parameter = set.front()->definition->parameters[i];
					const std::optional<Type> argument = sizeofExpressionType(*expression.arguments[i]);
					if (!argument || parameter.isReference || parameter.type.addressSpace != AddressSpace::None
						|| !converts(parameter.type, *argument)) {
						return std::nullopt;
					}
				}
				const Type& result = set.front()->definition->returnType;
				return result.scalar == ScalarKind::Void && result.namedType.empty() && !result.isPointer
					? std::nullopt : std::optional<Type>(result);
			}
			case ExpressionKind::Assign: {
				// The type of the place written; the value is never stored.
				const ExpressionKind target = expression.left->kind;
				if (target != ExpressionKind::Identifier && target != ExpressionKind::Member && target != ExpressionKind::Index) {
					return std::nullopt;
				}
				const std::optional<Type> place = sizeofExpressionType(*expression.left);
				const std::optional<Type> value = sizeofExpressionType(*expression.right);
				if (!place || !value || place->isConst || readOnly(*expression.left)
					|| (!expression.compoundOperator && !converts(*place, *value))) {
					return std::nullopt;
				}
				if (expression.compoundOperator) {
					// x op= y is x = x op y, which has to be a valid operator and convert back.
					if (place->isPointer) {
						return std::nullopt;
					}
					const std::optional<Type> result = binaryType(*expression.compoundOperator, *place, *value);
					if (!result || !converts(*place, *result)) {
						return std::nullopt;
					}
				}
				return place;
			}
			case ExpressionKind::Unary: {
				const std::optional<Type> operand = sizeofExpressionType(*expression.left);
				const bool incdec = expression.unaryOperator == UnaryOperator::PreIncrement
					|| expression.unaryOperator == UnaryOperator::PreDecrement
					|| expression.unaryOperator == UnaryOperator::PostIncrement
					|| expression.unaryOperator == UnaryOperator::PostDecrement;
				if (!operand || expression.unaryOperator == UnaryOperator::AddressOf
					|| !(plain(*operand) || (incdec && operand->isPointer && !operand->arrayLength))) {
					return std::nullopt;
				}
				switch (expression.unaryOperator) {
					case UnaryOperator::Not: return operand->isScalar() ? std::optional<Type>(scalarOf(ScalarKind::Bool)) : std::nullopt;
					case UnaryOperator::PreIncrement:
					case UnaryOperator::PreDecrement:
					case UnaryOperator::PostIncrement:
					case UnaryOperator::PostDecrement: {
						const ExpressionKind target = expression.left->kind;
						return (target == ExpressionKind::Identifier || target == ExpressionKind::Member
							|| target == ExpressionKind::Index) && operand->scalar != ScalarKind::Bool && !operand->isConst
							&& !readOnly(*expression.left) ? operand : std::nullopt;
					}
					case UnaryOperator::BitNot:
						return isFloatKind(operand->scalar) || operand->scalar == ScalarKind::Bool ? std::nullopt
							: std::optional<Type>(promoted(*operand));
					default: return promoted(*operand);
				}
			}
			case ExpressionKind::Binary: {
				const std::optional<Type> left = sizeofExpressionType(*expression.left);
				const std::optional<Type> right = sizeofExpressionType(*expression.right);
				return left && right ? binaryType(expression.binaryOperator, *left, *right) : std::nullopt;
			}
			default:
				return std::nullopt;
		}
	}

	bool Emitter::namesPointerParameter(const std::string& name) const {
		const FunctionDecl* function = _helper ? _helper : _entryPoint;
		return function && std::any_of(function->parameters.begin(), function->parameters.end(),
			[&](const Parameter& parameter) { return parameter.name == name && parameter.type.isPointer; });
	}

	// The size sizeof gives the expression's type; the expression itself is never evaluated. Only what
	// can be typed from the declarations, or an arithmetic result of such parts, is sized: a guess at
	// the type of anything else would be a silent wrong size.
	uint64_t Emitter::sizeofOperand(const Expression& operand) {
		const auto sized = [&](const Type& type) {
			if (type.scalar == ScalarKind::Void && type.namedType.empty() && !type.isPointer
				&& type.resource == ResourceKind::None && !type.isAuto) {
				throw CompileError("the type of this sizeof operand is not recorded; write sizeof(<type>)");
			}
			return sizeofType(type, _unit);
		};

		if (const std::optional<Type> type = sizeofExpressionType(operand)) {
			return sized(*type);
		}
		throw CompileError("sizeof of this expression is not lowered: its type is not one mslc reads "
			"from the declarations; write sizeof(<type>)");
	}

	// A place whose value is a whole vector.
	bool Emitter::isVectorPlace(const Expression& place) {
		const std::optional<Type> type = placeType(place);
		return type && !type->arrayLength && type->vectorWidth > 1 && !type->isMatrix();
	}

	// The type of a member or element chain of a computed value, by the struct declarations alone.
	std::optional<Type> Emitter::computedPartType(const Expression& part) {
		if (part.kind == ExpressionKind::Member) {
			const StructDecl* decl = structOf(*part.left);
			std::string name;
			const size_t field = decl ? fieldIndexOf(*decl, part.memberName, name) : 0;
			return decl && field != decl->fields.size() ? std::optional<Type>(decl->fields[field].type)
				: std::nullopt;
		}
		if (part.kind != ExpressionKind::Index) {
			return std::nullopt;
		}
		std::optional<Type> type = computedPartType(*part.left);
		if (!type) {
			return std::nullopt;
		}
		if (type->arrayLength) {
			type->dropOuterDimension();
		} else if (type->isMatrix()) {
			type->matrixColumns = 0;
		} else {
			return std::nullopt;
		}
		return type;
	}

	// A member or element of a temporary that a reference cannot bind. A column of a matrix is left
	// out: Metal's matrix operator[] gives an lvalue even for a prvalue, so Apple takes that.
	bool Emitter::bindsTemporary(const Expression& place) {
		if (!isTemporaryPart(place)) {
			return false;
		}
		for (const Expression* node = &place; node->kind == ExpressionKind::Member
			|| node->kind == ExpressionKind::Index; node = node->left.get()) {
			if (node->kind != ExpressionKind::Index) {
				continue;
			}
			// An element of a base that cannot be typed here (a call or a conditional) might be a matrix
			// column, so only a base known not to be a matrix may say temporary.
			const std::optional<Type> base = computedPartType(*node->left);
			if (!base || (!base->arrayLength && base->isMatrix())) {
				return false;
			}
		}
		return true;
	}

	// A component of a vector (v.x, v[1], m[1][0]), which no reference binds (Apple refuses it too).
	bool Emitter::isVectorComponent(const Expression& place) {
		if (place.kind == ExpressionKind::Member) {
			return !structOf(*place.left);
		}
		return place.kind == ExpressionKind::Index && isVectorPlace(*place.left);
	}

	// A reference into constant memory is passed as the value it names: the memory cannot change under
	// the callee. Its argument has to be a place of that space and of exactly the parameter's type.
	Id Emitter::constantReferenceArgument(const Expression& argument, const Parameter& parameter,
		const std::string& function, size_t position, Id prepared) {
		const std::string what = "argument " + std::to_string(position) + " of the call to \"" + function + "\"";
		const std::string reason = ", and parameter \"" + parameter.name + "\" is a reference to constant memory";
		if (argument.kind != ExpressionKind::Identifier && argument.kind != ExpressionKind::Index
			&& argument.kind != ExpressionKind::Member) {
			throw CompileError(what + " has to be a variable, an element or a member of constant memory" + reason);
		}
		if (isVectorComponent(argument)) {
			throw CompileError(what + " is a component of a vector, which a reference parameter cannot name" + reason);
		}
		if (!inConstantSpace(argument)) {
			throw CompileError(what + " is not in the constant address space" + reason
				+ "; Apple does not bind it to a value of another address space");
		}
		const auto tag = enumOf(argument);
		if (!parameter.type.enumName.empty()) {
			requireEnumValue(parameter.type.enumName, argument, what);
		}
		const Id value = prepared != InvalidId ? prepared : emitExpression(argument);
		// A struct read out of a buffer has the buffer's layout, which the by-value conversion maps.
		const bool structValue = !parameter.type.namedType.empty() && !parameter.type.isPointer;
		if ((!structValue && _builder.typeOf(value) != declaredTypeOf(parameter.type))
			|| (tag && *tag != parameter.type.enumName)) {
			throw CompileError(what + " is not the type of parameter \"" + parameter.name + "\", "
				+ typeName(parameter.type) + "; a conversion would make a temporary in another address space");
		}
		return value;
	}

	// A parameter is a value the body may assign to, as in C++, and the caller's
	// copy must not change, so each named one is copied into a variable of the
	// helper's own and the body reads and writes that.
	void Emitter::emitHelper(const HelperFunction& helper) {
		const FunctionDecl& definition = *helper.definition;

		_helper = &definition;
		_bindings.clear();
		bindFileScopeArrays(definition.globalsBefore);
		_terminated = false;
		_controlDepth = 0;

		_builder.setSection(spirv::Section::Functions);
		const Id function = helper.ids.at(helperScope(helper));
		_builder.emitDeclTypedAt(spirv::OpFunction, helper.returnType, function,
			{ kFunctionControlNone, helper.type });

		std::vector<Id> values;
		for (size_t i = 0; i < helper.parameterTypes.size(); ++i) {
			values.push_back(_builder.emitTyped(spirv::OpFunctionParameter, helper.parameterTypes[i], { }));

			// SPV_KHR_physical_storage_buffer wants exactly one of Aliased or Restrict on a
			// pointer parameter. Metal pointers may alias (f(out, out) is legal), so a
			// driver must not assume otherwise.
			const Parameter& parameter = definition.parameters[i];
			if (parameter.type.isPointer
				&& (parameter.type.addressSpace == AddressSpace::Device
					|| parameter.type.addressSpace == AddressSpace::Constant)) {
				_builder.emit(spirv::OpDecorate, { values.back(), static_cast<uint32_t>(spirv::Decoration::Aliased) });
			}
		}

		// A declaration is a function with no blocks, resolved when the module is linked.
		if (!definition.body) {
			_builder.emit(spirv::OpFunctionEnd, { });
			linkageAttributes(function, linkageName(definition), kLinkageImport);
			_helper = nullptr;
			return;
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

			if (parameter.type.isPointer) {
				if (parameter.type.addressSpace == AddressSpace::Device
					|| parameter.type.addressSpace == AddressSpace::Constant) {
					// A whole buffer, passed as the pointer value the argument gave:
					// p[i], *p and p->field walk it like an entry point's buffer.
					const auto [pointee, block] = bufferParamTypes(parameter.type);
					Binding bound;
					bound.id = values[i];
					bound.isPointer = true;
					bound.pointeeType = pointee;
					bound.bufferPointeeType = block;
					bound.storageClass = spirv::StorageClass::PhysicalStorageBuffer;
					bound.structType = parameter.type.namedType.empty()
						? nullptr : _unit.findStruct(parameter.type.namedType);
					bound.isBuffer = true;
					bound.ownAddress = true;
					bound.readOnly = parameter.type.isConst
						|| parameter.type.addressSpace == AddressSpace::Constant;
					bound.pointeeMsl = parameter.type;
					bound.pointeeMsl.isPointer = false;
					bound.pointeeMsl.isConst = false;
					_bindings[parameter.name] = bound;
					continue;
				}
				const Type array = boundArray(parameter.type, helper.arrayLengths[i]);
				bindLocal(parameter.name, values[i], arrayTypeOf(array), array);
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

	// The operand of a return in a function returning void: an expression of type void, such as a call to another
	// function returning void, is allowed and evaluated ([stmt.return]); any other value is an error.
	void Emitter::emitVoidReturnValue(const Expression& expression, const std::string& quoted) {
		if (isVoidValued(expression)) {
			emitExpressionStatement(expression);
			return;
		}
		if (expression.kind == ExpressionKind::Call) {
			_voidCallAllowed = true;
			const Id value = emitExpression(expression);
			_voidCallAllowed = false;
			if (_builder.typeOf(value) == _voidType) {
				return;
			}
		}
		throw CompileError(quoted + " returns void, so its return cannot carry a value");
	}

	void Emitter::emitHelperReturn(const Statement& statement) {
		const std::string quoted = "\"" + _helper->name + "\"";

		if (returnsVoid(*_helper)) {
			if (statement.expression) {
				emitVoidReturnValue(*statement.expression, quoted);
			}

			terminate(spirv::OpReturn, { });
			return;
		}

		if (!statement.expression) {
			throw CompileError(quoted + " returns " + typeName(_helper->returnType)
				+ ", so a return needs a value");
		}

		if (!_helper->returnType.enumName.empty()) {
			requireEnumValue(_helper->returnType.enumName, *statement.expression, "the value " + quoted + " returns");
		}
		const bool braced = statement.expression->kind == ExpressionKind::InitList && !statement.expression->constructType;
		if (braced && !_helper->returnType.enumName.empty()) {
			throw CompileError("a braced list is returned as " + typeName(_helper->returnType) + ", an enum, which is not lowered");
		}
		const Id value = braced ? emitInitListValue(_helper->returnType, *statement.expression)
			: emitExpression(*statement.expression);
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

		const bool isDerivative = builtin.shape == MathShape::Fwidth || builtin.shape == MathShape::Dfdx
			|| builtin.shape == MathShape::Dfdy;
		if (isDerivative && _entryPoint->stage != Stage::Fragment) {
			throw StageError(name + " is only available in fragment functions");
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

		if (isDerivative) {
			const spirv::Opcode opcode = builtin.shape == MathShape::Dfdx ? spirv::OpDPdx
				: builtin.shape == MathShape::Dfdy ? spirv::OpDPdy : spirv::OpFwidth;
			const Id type = _builder.typeOf(values[0]);
			if (!_types.isFloat(type)) {
				throw CompileError(name + " takes float or half scalars or vectors");
			}

			if (_types.bitWidth(type) == 16) {
				const Id float32Type = _types.vector(ScalarKind::Float, _types.vectorWidth(type));
				const Id widened = convert(values[0], type, float32Type);
				const Id result = _builder.emitTyped(opcode, float32Type, { widened });
				return convert(result, float32Type, type);
			}

			return _builder.emitTyped(opcode, type, { values[0] });
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
			|| builtin.shape == MathShape::Saturate || builtin.shape == MathShape::RoundHalfAway
			|| builtin.shape == MathShape::SignExact || builtin.shape == MathShape::FloatRemainder;
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

		if (builtin.shape == MathShape::RoundHalfAway) {
			return emitRoundHalfAway(values[0], type, bits);
		}
		if (builtin.shape == MathShape::SignExact) {
			return emitSignExact(values[0], type, bits);
		}
		if (builtin.shape == MathShape::FloatRemainder) {
			if (bits == 16) {
				// The quotient of a half remainder can pass the half range (65504 / 6), and a
				// driver that forms it in half precision answers infinity where Apple answers
				// the remainder, so it is computed in float and narrowed.
				const uint32_t width = _types.vectorWidth(type);
				const Id wide = width > 1 ? _types.vector(ScalarKind::Float, width) : _types.scalar(ScalarKind::Float);
				const Id a = convert(values[0], type, wide);
				const Id b = convert(values[1], type, wide);
				return convert(_builder.emitTyped(spirv::OpFRem, wide, { a, b }), wide, type);
			}
			return _builder.emitTyped(spirv::OpFRem, type, { values[0], values[1] });
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

	// round(x): the whole number nearest x, a half going away from zero. Trunc
	// gives the whole part; when the fraction is at least a half the result is one
	// further along the sign of x. Infinity and NaN come out as themselves, since
	// their fraction is NaN and so is never at least a half.
	Id Emitter::emitRoundHalfAway(Id value, Id type, uint32_t bits) {
		if (_glslSet == InvalidId) {
			std::vector<uint32_t> setName;
			spirv::Builder::appendString(setName, "GLSL.std.450");
			_glslSet = _builder.emitDecl(spirv::OpExtInstImport, setName);
		}

		const auto extended = [&](uint32_t instruction, Id operand) {
			return _builder.emitTyped(spirv::OpExtInst, type, { _glslSet, instruction, operand });
		};
		constexpr uint32_t kTrunc = 3;
		constexpr uint32_t kFAbs = 4;
		constexpr uint32_t kFSign = 6;

		const uint32_t width = _types.vectorWidth(type);
		const Id component = width > 1 ? _types.componentOf(type) : type;
		Id half = _builder.emitDeclTyped(spirv::OpConstant, component, { bits == 16 ? 0x3800u : 0x3F000000u });
		if (width > 1) {
			half = _builder.emitDeclTyped(spirv::OpConstantComposite, type,
				std::vector<uint32_t>(width, half));
		}

		const Id whole = extended(kTrunc, value);
		const Id fraction = _builder.emitTyped(spirv::OpFSub, type, { value, whole });
		const Id magnitude = extended(kFAbs, fraction);
		const Id boolType = width > 1 ? _types.vector(ScalarKind::Bool, width) : _boolType;
		const Id atLeastHalf = _builder.emitTyped(spirv::OpFOrdGreaterThanEqual, boolType, { magnitude, half });
		const Id further = _builder.emitTyped(spirv::OpFAdd, type, { whole, extended(kFSign, value) });
		return _builder.emitTyped(spirv::OpSelect, type, { atLeastHalf, further, whole });
	}

	// sign(x): FSign, except that a zero keeps its own sign and a NaN gives 0.
	Id Emitter::emitSignExact(Id value, Id type, uint32_t bits) {
		if (_glslSet == InvalidId) {
			std::vector<uint32_t> setName;
			spirv::Builder::appendString(setName, "GLSL.std.450");
			_glslSet = _builder.emitDecl(spirv::OpExtInstImport, setName);
		}

		const uint32_t width = _types.vectorWidth(type);
		const Id component = width > 1 ? _types.componentOf(type) : type;
		Id zero = _builder.emitDeclTyped(spirv::OpConstant, component, { 0u });
		if (width > 1) {
			zero = _builder.emitDeclTyped(spirv::OpConstantComposite, type,
				std::vector<uint32_t>(width, zero));
		}
		static_cast<void>(bits);

		constexpr uint32_t kFSign = 6;
		const Id boolType = width > 1 ? _types.vector(ScalarKind::Bool, width) : _boolType;
		const Id sign = _builder.emitTyped(spirv::OpExtInst, type, { _glslSet, kFSign, value });
		const Id isZero = _builder.emitTyped(spirv::OpFOrdEqual, boolType, { value, zero });
		const Id isNaN = _builder.emitTyped(spirv::OpIsNan, boolType, { value });
		const Id keptZero = _builder.emitTyped(spirv::OpSelect, type, { isZero, value, sign });
		return _builder.emitTyped(spirv::OpSelect, type, { isNaN, zero, keptZero });
	}

	// The constant a folded value becomes: a scalar as an OpConstant of its kind,
	// a bool as OpConstantTrue or OpConstantFalse, and a composite as the
	// constants making it up.
	Id Emitter::emitConstant(const FoldedConstant& folded) {
		if (folded.isComposite) {
			return constantStruct(folded.type, folded.parts);
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
				const auto local = expression.fileScope ? _bindings.end() : _bindings.find(expression.name);
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

			case ExpressionKind::Sizeof:
				folded.scalar = ScalarKind::ULong;
				folded.integer = sizeofOperand(*expression.left);
				return folded;

			case ExpressionKind::Binary: return foldBinary(expression);
			case ExpressionKind::Unary: return foldUnary(expression);
			case ExpressionKind::Conditional: return foldConditional(expression);
			case ExpressionKind::Construct:
				// (Mode)1, Mode(1) and static_cast<Mode>(1): the value converts to the
				// enum's integer type, whatever enum or integer it was.
				if (expression.constructType && !expression.constructType->enumName.empty()
					&& !expression.constructType->isPointer && !expression.constructType->arrayLength
					&& expression.arguments.size() == 1) {
					FoldedConstant operand = foldExpression(*expression.arguments.front());
					if (operand.isComposite) {
						throw CompileError("a constant's initialiser has to be a value that can be worked out "
							"from the declarations before it, and this is not one");
					}
					if (operand.scalar == ScalarKind::Bool) {
						operand.integer = operand.boolean ? 1 : 0;
						operand.scalar = ScalarKind::Int;
					} else if (isFloatKind(operand.scalar)) {
						// A truncated value outside the enum's underlying type is undefined in C++, and
						// Apple folds it through LLVM's undefined-behaviour rules; refuse it.
						const ScalarKind underlying = expression.constructType->scalar;
						const int width = mappingFor(underlying).width;
						const bool isSigned = isSignedInteger(underlying);
						const double truncated = std::trunc(operand.number);
						// An enum with no negative enumerator has an unsigned type in Apple's compiler
						// whatever mslc records, so a negative value is refused as well.
						if (!(truncated >= 0.0 && truncated < std::ldexp(1.0, isSigned ? width - 1 : width))) {
							throw CompileError("a float whose value does not fit the non-negative range of the enum "
								+ expression.constructType->enumName + " cannot be converted to it in a constant");
						}
						operand.integer = static_cast<uint64_t>(truncated);
						operand.scalar = ScalarKind::Int;
					}
					return convertConstant(operand, expression.constructType->scalar);
				}
				return foldScalarConstruct(expression);
			default: break;
		}

		throw CompileError("a constant's initialiser has to be a value that can be worked out "
			"from the declarations before it, and this is not one");
	}

	// c ? a : b of constants. Only the chosen arm is evaluated, but the result has the
	// type both arms meet in, so an arm that does not fold is emitted and lifted out again
	// for its type, which needs a function to emit into.
	FoldedConstant Emitter::foldConditional(const Expression& expression) {
		const FoldedConstant condition = foldExpression(*expression.left);
		if (condition.isComposite) {
			throw CompileError("a composite constant cannot be a condition");
		}
		const bool taken = condition.scalar == ScalarKind::Bool ? condition.boolean
			: isFloatKind(condition.scalar) ? condition.number != 0.0 : condition.integer != 0;
		const bool savedError = _foldDomainError;
		FoldedConstant arms[2];
		for (size_t i = 0; i < 2; ++i) {
			const bool chosen = (i == 0) == taken;
			_foldDomainError = false;
			try {
				arms[i] = foldExpression(*expression.arguments[i]);
			} catch (const CompileError&) {
				if (chosen) { throw; }
				// Only the chosen arm is evaluated; the other one contributes its type.
				// declareGlobals() folds every file-scope constant before any function body, so no block yet means file scope.
				if (_currentBlock == InvalidId) {
					throw CompileError("the type of an unchosen arm that is not a constant is not known outside a function");
				}
				std::optional<ScalarKind> kind;
				try {
					discardLifted([&] { kind = _types.scalarKindOf(_builder.typeOf(emitExpression(*expression.arguments[i]))); });
				} catch (const CompileError&) {
				}
				if (!kind) {
					throw CompileError("the type of a conditional constant needs both of its arms to be scalars");
				}
				arms[i] = FoldedConstant{};
				arms[i].scalar = *kind;
			}
			if (arms[i].isComposite) { throw CompileError("a composite constant cannot be a conditional arm"); }
			if (chosen && _foldDomainError) {
				throw CompileError("the chosen arm of a conditional constant is not a constant expression");
			}
		}
		_foldDomainError = savedError;
		FoldedConstant result = arms[taken ? 0 : 1];
		const ScalarKind first = arms[0].scalar;
		const ScalarKind second = arms[1].scalar;
		ScalarKind common = first;
		if (first != second) {
			if (isFloatKind(first) || isFloatKind(second)) {
				common = first == ScalarKind::Float || second == ScalarKind::Float ? ScalarKind::Float : ScalarKind::Half;
			} else {
				const auto promote = [](ScalarKind kind) { return kind == ScalarKind::Bool ? ScalarKind::Int : promotedKind(kind); };
				common = commonIntegerKind(promote(first), promote(second));
			}
		}
		if (result.scalar == ScalarKind::Bool && common != ScalarKind::Bool) {
			result.integer = result.boolean ? 1 : 0;
			result.scalar = ScalarKind::Int;
		}
		return result.scalar == common ? result : convertConstant(result, common);
	}

	// int(x), float(x) and bool(x) of a scalar constant. A float does not become an
	// integer here: a value outside the integer's range is undefined, and mslc
	// folds none of those.
	FoldedConstant Emitter::foldScalarConstruct(const Expression& expression) {
		const Type* type = expression.constructType ? &*expression.constructType : nullptr;
		if (!type || !type->isScalar() || type->isMatrix() || type->isPointer || type->isAtomic
			|| !type->namedType.empty() || !type->enumName.empty() || expression.arguments.size() != 1) {
			throw CompileError("a constant's initialiser has to be a value that can be worked out "
				"from the declarations before it, and this is not one");
		}

		return convertScalarForConstruct(foldExpression(*expression.arguments.front()), type->scalar);
	}

	// The value of a constant converted to the scalar type a constructor names. A float does not
	// become an integer unless it fits: a value outside the range is undefined, and mslc folds
	// none of those.
	FoldedConstant Emitter::convertScalarForConstruct(FoldedConstant folded, ScalarKind to) {
		if (folded.isComposite) {
			throw CompileError("a composite constant cannot be converted to a scalar");
		}

		if (to == ScalarKind::Bool) {
			folded.boolean = folded.scalar == ScalarKind::Bool ? folded.boolean
				: isFloatKind(folded.scalar) ? folded.number != 0.0 : folded.integer != 0;
			folded.scalar = to;
			return folded;
		}

		if (folded.scalar == ScalarKind::Bool) {
			folded.integer = folded.boolean ? 1 : 0;
			folded.scalar = ScalarKind::Int;
		}

		if (isFloatKind(folded.scalar) && !isFloatKind(to)) {
			// The truncated value has to fit the target, or the conversion is undefined
			// and so no constant expression.
			const double truncated = std::trunc(folded.number);
			const int width = mappingFor(to).width;
			const double limit = std::ldexp(1.0, isSignedInteger(to) ? width - 1 : width);
			const double lowest = isSignedInteger(to) ? -limit : 0.0;
			if (!(truncated >= lowest && truncated < limit)) {
				_foldDomainError = true;
				throw CompileError("a float whose value does not fit " + std::string(cxxTypeName(to))
					+ " cannot be converted to it in a constant");
			}
			folded.integer = truncated < 0 ? static_cast<uint64_t>(static_cast<int64_t>(truncated))
				: static_cast<uint64_t>(truncated);
			folded.scalar = ScalarKind::Long;
		}

		return convertConstant(folded, to);
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

	// A comparison of two scalar constants. A bool is an int 0 or 1 here, as at
	// run time, and the operands meet in the usual arithmetic conversions.
	FoldedConstant Emitter::foldComparison(BinaryOperator op, FoldedConstant left, FoldedConstant right) {
		for (FoldedConstant* operand: { &left, &right }) {
			if (operand->scalar == ScalarKind::Bool) {
				operand->scalar = ScalarKind::Int;
				operand->integer = operand->boolean ? 1 : 0;
			}
		}

		const auto compare = [&](auto l, auto r) {
			switch (op) {
				case BinaryOperator::Less: return l < r;
				case BinaryOperator::LessEqual: return l <= r;
				case BinaryOperator::Greater: return l > r;
				case BinaryOperator::GreaterEqual: return l >= r;
				case BinaryOperator::Equal: return l == r;
				default: return l != r;
			}
		};

		FoldedConstant folded;
		folded.scalar = ScalarKind::Bool;
		if (isFloatKind(left.scalar) || isFloatKind(right.scalar)) {
			const ScalarKind kind = left.scalar == ScalarKind::Float || right.scalar == ScalarKind::Float
				? ScalarKind::Float : ScalarKind::Half;
			const auto valueOf = [&](const FoldedConstant& operand) {
				return isFloatKind(operand.scalar) ? operand.number
					: integerAsKind(kind, operand.scalar, operand.integer);
			};
			folded.boolean = compare(valueOf(left), valueOf(right));
			return folded;
		}

		const ScalarKind common = commonIntegerKind(promotedKind(left.scalar), promotedKind(right.scalar));
		const uint64_t l = normalizeInteger(common, left.integer);
		const uint64_t r = normalizeInteger(common, right.integer);
		folded.boolean = isSignedInteger(common)
			? compare(static_cast<int64_t>(l), static_cast<int64_t>(r)) : compare(l, r);
		return folded;
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

		const BinaryOperator op = expression.binaryOperator;

		if (isComparisonOperator(op)) {
			return foldComparison(op, left, right);
		}

		if (left.scalar == ScalarKind::Bool || right.scalar == ScalarKind::Bool) {
			throw CompileError("an arithmetic or bitwise operator on a bool is not folded; "
				"convert the bool to an int first");
		}

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
			// Before C++20 a negative value cannot be shifted left, and the result has to
			// fit the unsigned type of the same width.
			if (op == BinaryOperator::ShiftLeft && isSignedInteger(folded.scalar)) {
				const uint64_t value = normalizeInteger(folded.scalar, left.integer);
				const bool leftNegative = static_cast<int64_t>(value) < 0;
				const bool overflows = width < 64
					? ((static_cast<unsigned __int128>(value) << count) >> width) != 0
					: (value << count) >> count != value;
				if (leftNegative || overflows) {
					_foldDomainError = true;
					throw CompileError("a left shift of a constant overflows");
				}
			}
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
			return foldMatrix(initializer, &type);
		}

		if (initializer.kind != ExpressionKind::InitList) {
			if (!type.namedType.empty()) {
				throw CompileError("the struct \"" + type.namedType + "\" is initialised with a "
					"braced list naming its fields, not with a single value");
			}

			// "int4(1)" and "float4(a, b, c, d)": a vector built by its own constructor, each
			// component converted as it is at run time.
			if (type.vectorWidth > 1 && initializer.kind == ExpressionKind::Construct
				&& initializer.constructType && !initializer.constructType->isMatrix()) {
				const Type& built = *initializer.constructType;
				if (built.scalar != type.scalar || built.vectorWidth != type.vectorWidth) {
					throw CompileError("the constructor of a " + std::string(scalarKindName(type.scalar))
						+ std::to_string(type.vectorWidth) + " constant has to be a "
						+ std::string(scalarKindName(type.scalar)) + std::to_string(type.vectorWidth)
						+ " already; mslc converts no vector");
				}
				FoldedConstant vector;
				vector.isComposite = true;
				vector.type = declaredTypeOf(type);
				for (const FoldedConstant& component: foldVectorConstructor(initializer)) {
					vector.parts.push_back(emitConstant(component));
				}
				return vector;
			}

			if (type.vectorWidth > 1) {
				throw CompileError("a " + std::string(scalarKindName(type.scalar))
					+ std::to_string(type.vectorWidth) + " constant is initialised with a braced "
					"list of its components or a constructor of its own type "
					"(not a single value, another constant or an expression)");
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

	// A scalar element of a matrix constant, as the matrix's own element kind. In a braced
	// list the value may not narrow; in a constructor it converts, as at run time.
	double Emitter::foldMatrixScalar(const Expression& expression, ScalarKind kind, bool braced) {
		if (isMatrixExpression(expression) || (expression.kind == ExpressionKind::Construct
			&& expression.constructType && expression.constructType->vectorWidth > 1)
			|| expression.kind == ExpressionKind::InitList) {
			throw CompileError("a matrix element has to be a scalar here");
		}

		const FoldedConstant folded = foldExpression(expression);
		if (folded.isComposite || folded.scalar == ScalarKind::Bool) {
			throw CompileError("a matrix or vector constant element has to be a float or half value; a bool or "
				"a composite is not folded into one");
		}
		if (braced) {
			requireNotNarrowing(folded, kind);
		}
		return convertConstant(folded, kind).number;
	}

	// float3(1, 2, 3), int4(1), int4(v2, 1, 2): the components of a vector constructor, each
	// converted to the vector's own scalar as at run time. A piece may be a nested vector
	// constructor; a count that is neither one scalar nor the width is refused.
	std::vector<FoldedConstant> Emitter::foldVectorConstructor(const Expression& expression) {
		const Type& type = *expression.constructType;
		if (type.vectorWidth < 2 || type.isMatrix() || type.isPointer || type.isAtomic || type.arrayLength
			|| !type.namedType.empty() || !type.enumName.empty()) {
			throw CompileError("a constant's initialiser has to be a value that can be worked out "
				"from the declarations before it, and this is not one");
		}

		std::vector<FoldedConstant> pieces;
		for (const ExpressionPtr& argument: expression.arguments) {
			if (argument->kind == ExpressionKind::Construct && argument->constructType
				&& argument->constructType->vectorWidth > 1 && !argument->constructType->isMatrix()) {
				// Apple converts a vector piece of another scalar type only when it is the whole
				// argument list; beside other arguments it is an error, so it is refused here too.
				if (argument->constructType->scalar != type.scalar && expression.arguments.size() != 1) {
					throw CompileError("a vector piece of a constructor has to have the vector's own scalar "
						"type unless it is the only argument");
				}
				for (const FoldedConstant& nested: foldVectorConstructor(*argument)) {
					pieces.push_back(convertScalarForConstruct(nested, type.scalar));
				}
			} else {
				pieces.push_back(convertScalarForConstruct(foldExpression(*argument), type.scalar));
			}
		}

		if (pieces.size() == 1 && expression.arguments.size() == 1) {
			return std::vector<FoldedConstant>(type.vectorWidth, pieces.front());
		}
		if (pieces.size() != type.vectorWidth) {
			throw CompileError("a vector built from " + std::to_string(pieces.size())
				+ " components needs " + std::to_string(type.vectorWidth));
		}
		return pieces;
	}

	// A column written as a vector constructor: float3(1, 2, 3), float3(0), float3(v2, 1).
	// The type has to be the column's own, as it does at run time.
	std::vector<double> Emitter::foldColumnConstructor(const Expression& expression, ScalarKind kind,
		uint32_t rows) {

		const Type& type = *expression.constructType;
		if (type.scalar != kind || type.vectorWidth != rows) {
			throw CompileError("a column of this matrix has to be a " + std::string(scalarKindName(kind))
				+ std::to_string(rows) + " already; mslc converts no column");
		}

		std::vector<double> pieces;
		for (const ExpressionPtr& argument: expression.arguments) {
			if (argument->kind == ExpressionKind::Construct && argument->constructType
				&& argument->constructType->vectorWidth > 1 && !argument->constructType->isMatrix()) {
				const Type& inner = *argument->constructType;
				if (inner.scalar != kind) {
					throw CompileError("a vector piece of a column has to have its component type; "
						"only a scalar piece is converted");
				}
				const std::vector<double> nested = foldColumnConstructor(*argument, kind, inner.vectorWidth);
				pieces.insert(pieces.end(), nested.begin(), nested.end());
			} else {
				pieces.push_back(foldMatrixScalar(*argument, kind, false));
			}
		}

		if (pieces.size() == 1 && expression.arguments.size() == 1) {
			return std::vector<double>(rows, pieces.front());
		}
		if (pieces.size() != rows) {
			throw CompileError("a column built from " + std::to_string(pieces.size())
				+ " components needs " + std::to_string(rows));
		}
		return pieces;
	}

	bool Emitter::isMatrixExpression(const Expression& expression) {
		switch (expression.kind) {
			case ExpressionKind::Construct: return expression.constructType && expression.constructType->isMatrix();
			case ExpressionKind::Identifier: {
				if (_bindings.count(expression.name)) { return false; }
				const auto found = _folded.find(expression.name);
				return found != _folded.end() && found->second.matrixColumns > 0;
			}
			case ExpressionKind::Binary:
				return isMatrixExpression(*expression.left) || isMatrixExpression(*expression.right);
			default: return false;
		}
	}

	// Emits the columns and the matrix as constants and keeps the numbers.
	FoldedConstant Emitter::matrixConstant(ScalarKind kind, uint32_t columns, uint32_t rows,
		std::vector<double> elements) {

		FoldedConstant matrix;
		matrix.isComposite = true;
		matrix.scalar = kind;
		matrix.matrixColumns = columns;
		matrix.matrixRows = rows;
		matrix.type = _types.matrix(kind, columns, rows);
		for (uint32_t column = 0; column < columns; ++column) {
			FoldedConstant vector;
			vector.isComposite = true;
			vector.type = _types.vector(kind, rows);
			for (uint32_t row = 0; row < rows; ++row) {
				FoldedConstant element;
				element.scalar = kind;
				element.number = elements[column * rows + row];
				vector.parts.push_back(emitConstant(element));
			}
			matrix.parts.push_back(emitConstant(vector));
		}
		matrix.elements = std::move(elements);
		return matrix;
	}

	// float3x3(...) as a constant: a scalar for the diagonal, a matrix of its own shape,
	// one vector constructor per column, or columns * rows scalars in column order.
	FoldedConstant Emitter::foldMatrixConstruct(const Expression& expression) {
		const Type& type = *expression.constructType;
		const std::string spelled = typeName(type);
		const uint32_t columns = type.matrixColumns;
		const uint32_t rows = type.vectorWidth;
		std::vector<double> elements(static_cast<size_t>(columns) * rows, 0.0);
		const auto& arguments = expression.arguments;

		if (arguments.size() == 1) {
			if (isMatrixExpression(*arguments.front())) {
				const FoldedConstant from = foldMatrix(*arguments.front(), nullptr);
				if (from.matrixColumns != columns || from.matrixRows != rows) {
					throw CompileError("no matching conversion from a matrix of another shape to " + spelled);
				}
				for (size_t i = 0; i < elements.size(); ++i) {
					elements[i] = roundedToKind(type.scalar, from.elements[i]);
				}
			} else {
				const double diagonal = foldMatrixScalar(*arguments.front(), type.scalar, false);
				for (uint32_t i = 0; i < columns && i < rows; ++i) {
					elements[i * rows + i] = diagonal;
				}
			}
		} else if (arguments.size() == static_cast<size_t>(columns) * rows) {
			for (size_t i = 0; i < arguments.size(); ++i) {
				elements[i] = foldMatrixScalar(*arguments[i], type.scalar, false);
			}
		} else if (arguments.size() == columns) {
			for (uint32_t column = 0; column < columns; ++column) {
				const Expression& argument = *arguments[column];
				if (argument.kind != ExpressionKind::Construct || !argument.constructType
					|| argument.constructType->vectorWidth < 2 || argument.constructType->isMatrix()) {
					throw CompileError("a column of " + spelled + " has to be a "
						+ std::string(scalarKindName(type.scalar)) + std::to_string(rows)
						+ " constructor in a constant");
				}
				const std::vector<double> values = foldColumnConstructor(argument, type.scalar, rows);
				std::copy(values.begin(), values.end(), elements.begin() + column * rows);
			}
		} else if (!arguments.empty()) {
			throw CompileError("no matching constructor for " + spelled + " built from "
				+ std::to_string(arguments.size()) + " values");
		}

		return matrixConstant(type.scalar, columns, rows, std::move(elements));
	}

	// A braced list for a matrix initialises one column per element, as it does in Apple's
	// compiler (measured on a GPU): a scalar fills the whole column, a vector is the column,
	// a nested brace gives the column's components and has to give all of them, columns
	// not written are zero, and elements past the last column are ignored.
	FoldedConstant Emitter::foldMatrixList(const Type& type, const Expression& initializer) {
		const uint32_t columns = type.matrixColumns;
		const uint32_t rows = type.vectorWidth;
		std::vector<double> elements(static_cast<size_t>(columns) * rows, 0.0);
		for (size_t column = 0; column < columns && column < initializer.elements.size(); ++column) {
			const InitializerElement& element = initializer.elements[column];
			if (!element.fieldName.empty()) {
				throw CompileError("a matrix's columns are given in order, but \"" + element.fieldName
					+ "\" names one");
			}

			const Expression& value = *element.value;
			std::vector<double> values;
			if (value.kind == ExpressionKind::InitList) {
				if (value.elements.size() != rows) {
					throw CompileError("chosen constructor is explicit in copy-initialization: a column of "
						+ typeName(type) + " takes " + std::to_string(rows) + " values in braces, found "
						+ std::to_string(value.elements.size()));
				}
				for (const InitializerElement& component: value.elements) {
					if (!component.fieldName.empty() || component.value->kind == ExpressionKind::InitList) {
						throw CompileError("a column's components are scalars given in order");
					}
					values.push_back(foldMatrixScalar(*component.value, type.scalar, true));
				}
			} else if (value.kind == ExpressionKind::Construct && value.constructType
				&& value.constructType->vectorWidth > 1 && !value.constructType->isMatrix()) {
				values = foldColumnConstructor(value, type.scalar, rows);
			} else {
				values.assign(rows, foldMatrixScalar(value, type.scalar, true));
			}
			std::copy(values.begin(), values.end(), elements.begin() + column * rows);
		}

		return matrixConstant(type.scalar, columns, rows, std::move(elements));
	}

	// A * B of two constant matrices of one element kind: column j of the result is
	// sum over k of A's column k times B[j][k], each product and each sum rounded to the
	// element kind in turn, which is what Apple's compiler produced (measured on a GPU).
	FoldedConstant Emitter::foldMatrixProduct(const Expression& expression) {
		if (expression.binaryOperator != BinaryOperator::Multiply) {
			throw CompileError("this operator on constant matrices is not folded yet; only a product is");
		}
		if (!isMatrixExpression(*expression.left) || !isMatrixExpression(*expression.right)) {
			throw CompileError("a matrix constant is multiplied by another matrix constant here; "
				"a product with a scalar or a vector is not folded yet");
		}

		const FoldedConstant a = foldMatrix(*expression.left, nullptr);
		const FoldedConstant b = foldMatrix(*expression.right, nullptr);
		if (a.scalar != b.scalar) {
			throw CompileError("the matrices multiplied have different element types");
		}
		if (a.matrixColumns != b.matrixRows) {
			throw CompileError("cannot multiply a " + std::to_string(a.matrixColumns) + "-column matrix by a "
				+ std::to_string(b.matrixRows) + "-row one");
		}

		const ScalarKind kind = a.scalar;
		const uint32_t columns = b.matrixColumns;
		const uint32_t rows = a.matrixRows;
		std::vector<double> elements;
		for (uint32_t j = 0; j < columns; ++j) {
			for (uint32_t i = 0; i < rows; ++i) {
				double sum = 0.0;
				for (uint32_t k = 0; k < a.matrixColumns; ++k) {
					const double product = roundedToKind(kind, a.elements[k * rows + i] * b.elements[j * b.matrixRows + k]);
					sum = k == 0 ? product : roundedToKind(kind, sum + product);
				}
				elements.push_back(sum);
			}
		}

		return matrixConstant(kind, columns, rows, std::move(elements));
	}

	// A matrix constant's initialiser: a braced list for the declared type, a constructor,
	// the name of a matrix constant declared above, or a product of two of those.
	FoldedConstant Emitter::foldMatrix(const Expression& expression, const Type* expected) {
		FoldedConstant folded;
		switch (expression.kind) {
			case ExpressionKind::InitList:
				if (!expected) {
					throw CompileError("a braced list needs the matrix type it initialises");
				}
				folded = foldMatrixList(*expected, expression);
				break;
			case ExpressionKind::Construct:
				if (!expression.constructType || !expression.constructType->isMatrix()) {
					throw CompileError("a matrix constant is initialised with a matrix, not a "
						+ (expression.constructType ? typeName(*expression.constructType) : std::string("value")));
				}
				folded = foldMatrixConstruct(expression);
				break;
			case ExpressionKind::Identifier: {
				const auto found = _bindings.count(expression.name) ? _folded.end() : _folded.find(expression.name);
				if (found == _folded.end() || found->second.matrixColumns == 0) {
					throw CompileError("\"" + expression.name + "\" is not a matrix constant this source "
						"declares before this one");
				}
				folded = found->second;
				break;
			}
			case ExpressionKind::Binary:
				folded = foldMatrixProduct(expression);
				break;
			default:
				throw CompileError("a matrix constant's initialiser has to be a matrix constructor, a braced list "
					"or a product of matrix constants");
		}

		if (expected && (folded.scalar != expected->scalar || folded.matrixColumns != expected->matrixColumns
			|| folded.matrixRows != expected->vectorWidth)) {
			throw CompileError("no viable conversion to " + typeName(*expected) + " from a matrix of another type");
		}
		return folded;
	}

	// A file-scope "constant" is a compile-time constant, so its initialiser is
	// folded and the result declared as one of the module's constants, with no
	// binding of its own.
	Id Emitter::declareGlobalConstant(const VariableDeclaration& declaration) {
		if (declaration.type.isMatrix() && declaration.type.isConstexpr) {
			throw CompileError("constexpr variable \"" + declaration.name
				+ "\" must be initialized by a constant expression");
		}
		if (!declaration.type.enumName.empty() && declaration.initializer->kind != ExpressionKind::InitList) {
			requireEnumValue(declaration.type.enumName, *declaration.initializer,
				"the constant \"" + declaration.name + "\"");
		}
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

	// A [[function_constant(n)]] is recorded here and declared where it is first read. Of the other constants,
	// only "constant bool b = is_function_constant_defined(c);" may read one, as another name for whether c
	// is defined: an initialiser computed from a value given at pipeline creation is not folded.
	void Emitter::declareFunctionConstant(const VariableDeclaration& declaration) {
		for (const auto& [name, other]: _functionConstants) {
			if (*other.declaration->functionConstantIndex == *declaration.functionConstantIndex) {
				throw CompileError("the function constants \"" + name + "\" and \"" + declaration.name
					+ "\" have the same index " + std::to_string(*declaration.functionConstantIndex));
			}
		}
		_functionConstants[declaration.name].declaration = &declaration;
	}

	// "constant bool b = is_function_constant_defined(c);" makes b another name for whether c is defined. Any
	// other initialiser that reads a function constant is refused.
	void Emitter::declareFunctionConstantAlias(const VariableDeclaration& declaration, const std::string& read) {
		const Expression& initializer = *declaration.initializer;
		const bool alias = initializer.kind == ExpressionKind::Call && initializer.left
			&& initializer.left->kind == ExpressionKind::Identifier
			&& builtinCalleeName(initializer.left->name) == "__metal_is_function_constant_defined"
			&& initializer.arguments.size() == 1 && initializer.arguments[0]->kind == ExpressionKind::Identifier
			&& _functionConstants.count(initializer.arguments[0]->name);
		if (!alias) {
			throw CompileError("the constant \"" + declaration.name + "\" is initialised from the function constant \""
				+ read + "\", and the only such initialiser mslc lowers is is_function_constant_defined(" + read + ")");
		}
		const Type& type = declaration.type;
		if (type.scalar != ScalarKind::Bool || type.vectorWidth > 1 || type.isPointer || type.arrayLength) {
			throw CompileError("the constant \"" + declaration.name + "\" holds is_function_constant_defined(...) "
				"and has to be a bool");
		}
		_functionConstantDefinedAliases[declaration.name] = initializer.arguments[0]->name;
	}

	// The function constant a name stands for, through "constant bool b = is_function_constant_defined(c);" too.
	const std::string* Emitter::functionConstantNamed(const std::string& name) const {
		const auto constant = _functionConstants.find(name);
		if (constant != _functionConstants.end()) {
			return &constant->first;
		}
		const auto alias = _functionConstantDefinedAliases.find(name);
		return alias == _functionConstantDefinedAliases.end() ? nullptr : &alias->second;
	}

	// The value of a function constant, or with `defined` the bool that says whether it was given one, as a
	// specialization constant that defaults to zero (false).
	Id Emitter::functionConstantValue(const std::string& name, bool defined) {
		FunctionConstant& constant = _functionConstants.at(name);
		_functionConstantReads[_helper ? _helper : _entryPoint].insert(name);
		const uint32_t index = *constant.declaration->functionConstantIndex;
		if (constant.value == InvalidId) {
			const ScalarKind kind = constant.declaration->type.scalar;
			const Id type = _types.scalar(kind);
			if (kind == ScalarKind::Bool) {
				constant.value = _builder.emitDeclTyped(spirv::OpSpecConstantFalse, type, { });
			} else if (kind == ScalarKind::Long || kind == ScalarKind::ULong) {
				constant.value = _builder.emitDeclTyped(spirv::OpSpecConstant, type, { 0u, 0u });
			} else {
				constant.value = _builder.emitDeclTyped(spirv::OpSpecConstant, type, { 0u });
			}
			_builder.emit(spirv::OpDecorate, { constant.value, static_cast<uint32_t>(spirv::Decoration::SpecId),
				kFunctionConstantSpecializationID + index });
			constant.defined = _builder.emitDeclTyped(spirv::OpSpecConstantFalse, _types.scalar(ScalarKind::Bool), { });
			_builder.emit(spirv::OpDecorate, { constant.defined, static_cast<uint32_t>(spirv::Decoration::SpecId),
				kFunctionConstantDefinedSpecializationID + index });
		}
		return defined ? constant.defined : constant.value;
	}

	Id Emitter::emitIsFunctionConstantDefined(const Expression& call) {
		if (call.arguments.size() != 1 || call.arguments[0]->kind != ExpressionKind::Identifier
			|| !_functionConstants.count(call.arguments[0]->name) || findResourceBinding(call.arguments[0]->name)) {
			throw CompileError("is_function_constant_defined takes the name of a function constant");
		}
		return functionConstantValue(call.arguments[0]->name, true);
	}

	// The reflection of the function constants the entry point reads, itself or in a function it reaches, by index.
	std::string Emitter::functionConstantsJson() {
		std::map<uint32_t, std::string> byIndex;
		std::set<const FunctionDecl*> reached { _entryPoint };
		std::vector<const FunctionDecl*> pending { _entryPoint };
		while (!pending.empty()) {
			const FunctionDecl* function = pending.back();
			pending.pop_back();
			for (const std::string& name: _functionConstantReads[function]) {
				byIndex[*_functionConstants.at(name).declaration->functionConstantIndex] = name;
			}
			for (const FunctionDecl* callee: _functionCallees[function]) {
				if (reached.insert(callee).second) {
					pending.push_back(callee);
				}
			}
		}
		std::string json;
		for (const auto& [index, name]: byIndex) {
			const VariableDeclaration& declaration = *_functionConstants.at(name).declaration;
			json += std::string(json.empty() ? "" : ",") + "\n\t\t\t\t{ \"name\": \"" + name + "\", \"type\": \""
				+ scalarKindName(declaration.type.scalar) + "\", \"index\": " + std::to_string(index)
				+ ", \"spec_id\": " + std::to_string(kFunctionConstantSpecializationID + index)
				+ ", \"defined_spec_id\": " + std::to_string(kFunctionConstantDefinedSpecializationID + index) + " }";
		}
		return json;
	}

	// The module's own declarations, in source order. A constant is declared
	// before the entry point that reads it, which is what lets the folder resolve
	// one constant's initialiser against the ones above it.
	void Emitter::declareGlobals() {
		for (size_t position = 0; position < _unit.globals.size(); ++position) {
			const VariableDeclaration& global = _unit.globals[position];
			if (global.sampler) {
				_fileScopeSamplers[global.name] = &global;
				continue;
			}
			_builder.setSection(spirv::Section::TypesGlobals);
			if (global.functionConstantIndex) {
				declareFunctionConstant(global);
				continue;
			}
			if (global.initializer && !_functionConstants.empty()) {
				std::set<std::string> read;
				collectIdentifiers(*global.initializer, read);
				const auto constant = std::find_if(read.begin(), read.end(),
					[&](const std::string& name) { return functionConstantNamed(name) != nullptr; });
				if (constant != read.end()) {
					declareFunctionConstantAlias(global, *constant);
					continue;
				}
			}
			if (global.type.arrayLength) {
				declareFileScopeArray(global, position);
				continue;
			}
			declareGlobalConstant(global);
		}
	}

	// A file-scope constant array is a Private variable with a constant initialiser, which
	// no store reaches. Vulkan has no way to index a constant composite by a run-time index,
	// so the alternative is a Function copy at every use; one module-scope variable is read
	// in place by a kernel and a helper alike, and indexing it is one access chain.
	void Emitter::declareFileScopeArray(const VariableDeclaration& declaration, size_t position) {
		const Type& type = declaration.type;
		if (type.isPointer || type.resource != ResourceKind::None) {
			throw CompileError("a file-scope array of pointers or resources is not lowered yet");
		}
		if (!declaration.initializer || declaration.initializer->kind != ExpressionKind::InitList) {
			throw CompileError("array initializer must be an initializer list");
		}

		ArrayLeaves leaves;
		leaves.element = [&](const Type& leaf, const Expression* entry) {
			if (!entry) {
				return _types.zero(declaredTypeOf(leaf));
			}
			if (!leaf.enumName.empty()) {
				requireEnumValue(leaf.enumName, *entry, "an element of the initializer list");
			}
			_foldDomainError = false;
			const FoldedConstant folded = foldInitializer(leaf, *entry);
			if (_foldDomainError && type.isConstexpr) {
				throw CompileError("constexpr variable \"" + declaration.name + "\" must be initialized by a "
					"constant expression");
			}
			_foldDomainError = false;
			return emitConstant(folded);
		};
		leaves.compose = [&](Id composite, const std::vector<uint32_t>& parts) {
			return constantStruct(composite, parts);
		};
		size_t consumed = 0;
		const Id initial = emitArrayInit(type, declaration.initializer->elements, consumed, true, leaves);

		FileScopeArray array;
		array.name = declaration.name;
		array.position = position;
		array.type = _builder.typeOf(initial);
		array.msl = type;
		array.variable = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Private, array.type),
			{ static_cast<uint32_t>(spirv::StorageClass::Private), initial });
		_fileScopeArrays.push_back(array);
	}

	// The arrays a function can see are the ones declared above it. A parameter or local
	// of the same name is bound afterwards and wins.
	void Emitter::bindFileScopeArrays(size_t declaredBefore) {
		_fileScopeBound.clear();
		for (const FileScopeArray& array: _fileScopeArrays) {
			if (array.position >= declaredBefore) {
				continue;
			}
			Binding binding;
			binding.id = array.variable;
			binding.isPointer = true;
			binding.pointeeType = array.type;
			binding.storageClass = spirv::StorageClass::Private;
			binding.pointeeMsl = array.msl;
			binding.readOnly = true;
			_bindings[array.name] = binding;
			_fileScopeBound[array.name] = binding;
		}
	}

	// "::name" names what the file declares, so the entry that a local of that name
	// holds is set aside for the expression; see FileScopeView.
	// Evaluating anything else while any is in force suspends them all.
	std::unique_ptr<FileScopeView> Emitter::viewFileScope(const Expression& expression) {
		const Expression* root = &expression;
		while ((root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) && root->left) {
			root = root->left.get();
		}
		const auto hidden = [&](const std::string& name) {
			return std::any_of(_fileScopeViews.begin(), _fileScopeViews.end(),
				[&](const FileScopeView* view) { return view->hides(name); });
		};
		if (root->kind == ExpressionKind::Identifier && root->fileScope) {
			if (hidden(root->name)) {
				return std::make_unique<FileScopeView>(_fileScopeViews);
			}
			const auto array = _fileScopeBound.find(root->name);
			return std::make_unique<FileScopeView>(_bindings, _fileScopeViews, root->name,
				array == _fileScopeBound.end() ? nullptr : &array->second);
		}
		const bool anyHidden = std::any_of(_fileScopeViews.begin(), _fileScopeViews.end(),
			[](const FileScopeView* view) { return view->hiding(); });
		return std::make_unique<FileScopeView>(_fileScopeViews, anyHidden);
	}

	Id Emitter::emitExpression(const Expression& expression) {
		const auto fileScopeView = viewFileScope(expression);
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
			case ExpressionKind::Comma:
				emitExpressionStatement(*expression.left);
				return emitExpression(*expression.right);
			case ExpressionKind::Sizeof: {
				const uint64_t size = sizeofOperand(*expression.left);
				return _builder.emitDeclTyped(spirv::OpConstant, _types.scalar(ScalarKind::ULong),
					{ static_cast<uint32_t>(size), static_cast<uint32_t>(size >> 32) });
			}
			case ExpressionKind::InitList:
				// S{...} names its struct; a bare list does not and is only an initialiser.
				if (expression.constructType) {
					return emitInitListValue(*expression.constructType, expression);
				}
				break;
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
			case ParameterAttributes::Builtin::ThreadsPerThreadgroup:
				return "threads_per_threadgroup";
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
			case ParameterAttributes::Builtin::ThreadsPerThreadgroup:
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
	// constant parameters: an unbinding parameter takes the lowest free buffer
	// index (declareParameters). add.metal relies on this, since none of its
	// pointers carry an attribute.
	std::vector<const Parameter*> assignImplicitBindings(const FunctionDecl& entryPoint) {
		std::vector<const Parameter*> bound;

		for (const Parameter& parameter: entryPoint.parameters) {
			if (parameter.attributes.bufferIndex || parameter.attributes.textureIndex
				|| parameter.attributes.samplerIndex || parameter.attributes.builtin
				|| parameter.attributes.stageIn || parameter.attributes.colorIndex
				|| parameter.attributes.threadgroupIndex
				|| parameter.type.addressSpace == AddressSpace::Threadgroup
				|| parameter.type.resource != ResourceKind::None) {
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

		// An unattributed buffer takes the lowest index that no [[buffer(n)]] anywhere in the list
		// claims and no earlier unattributed buffer took, not its position among them.
		std::map<const Parameter*, uint32_t> implicitBufferIndices;
		{
			std::set<uint32_t> taken;
			for (const Parameter& parameter: _entryPoint->parameters) {
				if (parameter.attributes.bufferIndex) { taken.insert(*parameter.attributes.bufferIndex); }
			}
			uint32_t next = 0;
			for (const Parameter* parameter: implicitlyBound) {
				while (taken.count(next)) { ++next; }
				if (next > 30) {
					throw CompileError("no 'buffer' resource location is available for parameter \""
						+ parameter->name + "\": every index up to 30 is taken");
				}
				implicitBufferIndices[parameter] = next;
				taken.insert(next);
			}
		}

		// Apple numbers the threadgroup arguments apart from the buffers: an explicit
		// [[threadgroup(n)]] keeps n, and one without takes the lowest free index, in order.
		std::map<const Parameter*, uint32_t> threadgroupIndices;
		{
			std::set<uint32_t> taken;
			for (const Parameter& parameter: _entryPoint->parameters) {
				if (!parameter.attributes.threadgroupIndex) { continue; }
				if (parameter.type.addressSpace != AddressSpace::Threadgroup
					|| !(parameter.type.isPointer || parameter.isReference) || parameter.attributes.bufferIndex) {
					throw CompileError("parameter \"" + parameter.name + "\" has [[threadgroup(n)]], which a "
						"pointer or reference in the threadgroup address space takes, with no other index");
				}
				if (!taken.insert(*parameter.attributes.threadgroupIndex).second) {
					throw CompileError("parameter \"" + parameter.name + "\" reserves threadgroup index "
						+ std::to_string(*parameter.attributes.threadgroupIndex) + ", which another argument "
						"already has");
				}
				threadgroupIndices[&parameter] = *parameter.attributes.threadgroupIndex;
			}
			uint32_t next = 0;
			for (const Parameter& parameter: _entryPoint->parameters) {
				if (parameter.type.addressSpace != AddressSpace::Threadgroup || parameter.attributes.bufferIndex
					|| threadgroupIndices.count(&parameter)) {
					continue;
				}
				if (!(parameter.type.isPointer || parameter.isReference)) {
					const ParameterAttributes& a = parameter.attributes;
					if (a.stageIn || a.builtin || a.colorIndex || a.textureIndex || a.samplerIndex) { continue; }
					throw CompileError("parameter \"" + parameter.name + "\" is a threadgroup value; a parameter in "
						"the threadgroup address space is a pointer or a reference");
				}
				while (taken.count(next)) { ++next; }
				if (next > 30) {
					throw CompileError("parameter \"" + parameter.name + "\" is past the last threadgroup index, 30");
				}
				taken.insert(next);
				threadgroupIndices[&parameter] = next;
			}
		}

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

			if (parameter.attributes.colorIndex) {
				if (_entryPoint->stage != Stage::Fragment) {
					throw CompileError("parameter \"" + parameter.name + "\" is [[color(n)]], which only a "
						"fragment function reads");
				}
				if (parameter.isReference || parameter.type.isPointer) {
					throw CompileError("parameter \"" + parameter.name + "\" is [[color(n)]], which Apple's "
						"compiler takes by value");
				}
				requireThreadAddressSpace(parameter, "[[color(n)]]");
				_attachmentParameters.push_back({ &parameter, declareFramebufferFetch(parameter.type,
					*parameter.attributes.colorIndex, parameter.name, "parameter \"" + parameter.name + "\"") });
				continue;
			}

			if (parameter.attributes.builtin) {
				requireThreadAddressSpace(parameter, "a builtin");

				if (*parameter.attributes.builtin == ParameterAttributes::Builtin::ThreadsPerThreadgroup) {
					if (_entryPoint->stage != Stage::Kernel) {
						throw CompileError("builtin \"threads_per_threadgroup\" is not available in a "
							+ std::string(_entryPoint->stage == Stage::Vertex ? "vertex" : "fragment")
							+ " function (parameter \"" + parameter.name + "\")");
					}

					const Type& type = parameter.type;
					const bool width = type.vectorWidth <= 3;
					const bool unsignedInt = type.scalar == ScalarKind::UInt || type.scalar == ScalarKind::UShort;
					if (!width || !unsignedInt || type.isPointer || type.isMatrix() || type.arrayLength
						|| !type.namedType.empty() || type.isPacked || parameter.isReference) {
						throw CompileError("parameter \"" + parameter.name + "\" is a " + typeName(type)
							+ ", which is not valid for [[threads_per_threadgroup]]; it takes uint or ushort, "
								"or a vector of two or three of them");
					}

					if (_threadsPerThreadgroup) {
						throw CompileError("[[threads_per_threadgroup]] is on both \""
							+ _threadsPerThreadgroup->name + "\" and \"" + parameter.name
							+ "\"; a function takes it once");
					}

					_threadsPerThreadgroup = &parameter;
					continue;
				}
				const spirv::BuiltInValue spvBuiltin = [&]() {
					switch (*parameter.attributes.builtin) {
						case ParameterAttributes::Builtin::ThreadPositionInGrid:
							return spirv::BuiltIn::GlobalInvocationId;
						case ParameterAttributes::Builtin::ThreadPositionInThreadgroup:
							return spirv::BuiltIn::LocalInvocationId;
						// The linear index within the workgroup, which is what
						// Metal names, rather than a position within it.
						case ParameterAttributes::Builtin::ThreadIndexInThreadgroup:
							return spirv::BuiltIn::LocalInvocationIndex;
						case ParameterAttributes::Builtin::ThreadgroupPositionInGrid:
							return spirv::BuiltIn::WorkgroupId;
						// Vulkan forbids VertexId and InstanceId. The Index forms
						// include the base vertex and instance, as Metal's do.
						case ParameterAttributes::Builtin::VertexID:
							return spirv::BuiltIn::VertexIndex;
						case ParameterAttributes::Builtin::InstanceID:
							return spirv::BuiltIn::InstanceIndex;
						case ParameterAttributes::Builtin::FragCoord:
						case ParameterAttributes::Builtin::Position:
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
					// A scalar, unlike the compute ids above: LocalInvocationIndex
					// is a linear index within the workgroup, so there is no
					// component to extract and declaring it uvec3 would be wrong.
					case spirv::BuiltIn::LocalInvocationIndex:
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

				// A direct FragCoord-mapped parameter (either MSL spelling) has to
				// declare a float4: nothing else reinterprets the builtin's fixed
				// Vulkan type, so a narrower or wider declaration would silently
				// alias or misread it. At most one FragCoord source (this
				// parameter, another parameter, or a stage-struct [[position]]
				// field) may exist per fragment entry point, since Vulkan forbids
				// two Input variables decorated with the same BuiltIn.
				if (spvBuiltin == spirv::BuiltIn::FragCoord) {
					if (declaredTypeOf(parameter.type) != _types.vector(ScalarKind::Float, 4)) {
						throw CompileError("parameter \"" + parameter.name + "\" is [[" +
							builtinName(*parameter.attributes.builtin) + "]], which has to be a float4");
					}
					if (_hasFragCoordInput) {
						throw CompileError("parameter \"" + parameter.name + "\" is [[" +
							builtinName(*parameter.attributes.builtin) + "]], which collides with this "
							"function's other fragment position input");
					}
					_hasFragCoordInput = true;
				}

				// Apple takes a compute id as uint or ushort, scalar or up to three
				// wide, and rejects int, float and a fourth lane.
				const bool computeId = spvBuiltin == spirv::BuiltIn::GlobalInvocationId
					|| spvBuiltin == spirv::BuiltIn::LocalInvocationId
					|| spvBuiltin == spirv::BuiltIn::WorkgroupId;
				if (computeId && (parameter.type.vectorWidth > 3 || parameter.type.vectorWidth == 1
					|| (parameter.type.scalar != ScalarKind::UInt && parameter.type.scalar != ScalarKind::UShort))) {
					throw CompileError("parameter \"" + parameter.name + "\" is [[" +
						builtinName(*parameter.attributes.builtin) + "]], which has to be a uint or ushort "
						"scalar or a vector of two or three of them");
				}

				// LocalInvocationIndex is the odd one out: it is a linear index
				// rather than a position, so Apple takes it as a uint or ushort
				// SCALAR and rejects every vector, including uint2 and uint3. Left
				// to the compute-id rule above it would over-accept float, int, bool
				// and half, and a uint3 would fall through to the swizzle check and
				// be reported as ".x swizzles a value that is not a vector", which
				// names neither the attribute nor the real problem.
				//
				// A scalar is vectorWidth 0, not 1: the compute-id rule above reads
				// "== 1" as the scalar to reject.
				if (spvBuiltin == spirv::BuiltIn::LocalInvocationIndex
					&& (parameter.type.vectorWidth > 0
						|| (parameter.type.scalar != ScalarKind::UInt
							&& parameter.type.scalar != ScalarKind::UShort))) {
					throw CompileError("parameter \"" + parameter.name + "\" is [[" +
						builtinName(*parameter.attributes.builtin) + "]], which has to be a uint or ushort "
						"scalar");
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
				const uint32_t declaredLanes = std::max(parameter.type.vectorWidth, 1u);
				if (computeId) {
					binding.leadingLanes = declaredLanes;
					binding.pointeeMsl = parameter.type;
				}
				binding.readOnly = parameter.type.isConst;
				_bindings[parameter.name] = binding;
				_interface.push_back(id);
				if (spvBuiltin == spirv::BuiltIn::GlobalInvocationId) {
					_globalInvocationVariable = id;
				}
				continue;
			}

			if (threadgroupIndices.count(&parameter)) {
				if (_entryPoint->stage != Stage::Kernel) {
					throw CompileError("parameter \"" + parameter.name + "\" is a threadgroup parameter; "
						"threadgroup parameters are only supported on kernel functions");
				}
				declareThreadgroupArgument(parameter, index, threadgroupIndices[&parameter]);
				continue;
			}

			// An unbinding parameter takes the lowest buffer index no attribute claims.
			uint32_t bindingIndex = 0;
			if (parameter.attributes.bufferIndex) {
				bindingIndex = *parameter.attributes.bufferIndex;
			} else {
				const auto it = implicitBufferIndices.find(&parameter);
				if (it == implicitBufferIndices.end()) {
					throw CompileError("parameter \"" + parameter.name + "\" has no [[buffer]], "
						"[[texture]], [[sampler]] or builtin attribute, and no address space mslc "
						"can bind, so there is nothing to bind it to");
				}
				bindingIndex = it->second;
			}

		// A threadgroup pointer with no [[buffer]] is an argument of its own and was
		// handled above; with [[buffer]] Apple rejects it, and so does the check below.
		const AddressSpace space = parameter.type.addressSpace;
		if (space == AddressSpace::None) {
			throw CompileError("parameter \"" + parameter.name + "\": pointer parameter needs an "
				"explicit address space (device or constant)");
		}
		if (space != AddressSpace::Device && space != AddressSpace::Constant) {
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
			pointeeType = declaredTypeOf(parameter.type);
		}

		// A buffer parameter's address is a member of the binding-0 block rather
		// than a descriptor of its own, so what a member points at is the
		// buffer's own type. A pointer parameter is a whole buffer, and
		// OpAccessChain rejects a non-composite base, so the element type is
		// wrapped in { T runtime_array[] } and the parameter indexes through
		// that -- for a struct element as much as for a scalar one.
		Id bufferPointee = pointeeType;
		// A bool is a byte in a buffer, so the reference's own memory is that byte (or
		// the bytes of a bool vector), mapped back to a bool where it is used.
		const bool matrixReference = !parameter.type.isPointer && parameter.type.isMatrix();
		if (matrixReference) {
			// { matrix m; }: the matrix is member 0, which carries its layout.
			bufferPointee = _types.matrixReferenceStruct(pointeeType);
		}
		const bool boolReference = !parameter.type.isPointer && _types.isBool(pointeeType);
		if (boolReference) {
			bufferPointee = _types.boolStorage(pointeeType);
		}
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
		if (boolReference) {
			_bufferBoolTypes[memberIndex] = pointeeType;
		}
		if (matrixReference) {
			_bufferMatrixTypes[memberIndex] = pointeeType;
		}

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
		_threadgroupLocals.clear();
		reserveThreadgroupLocals(*_entryPoint->body);
		reserveFileScopeSamplers();
		declareGridMaskInput();
		// Last, so the constexpr samplers keep the bindings right after the textures and samplers,
		// the order indium binds by; an attachment is not part of Iridium's argument list.
		declareAttachments();
	}

	// A kernel that skips the threads outside the grid reads the thread position in the grid, which has to
	// be an Input variable in the entry point's interface, so it is declared with the parameters. A kernel
	// that has a barrier anywhere cannot skip threads (a thread that returns before a barrier leaves
	// the rest of its workgroup waiting), and neither can one in a module with no mask requested.
	void Emitter::declareGridMaskInput() {
		_masksGridSize = false;
		if (!_options.maskGridSize || _entryPoint->stage != Stage::Kernel) {
			return;
		}
		if (callsBarrier(*_entryPoint->body)) {
			return;
		}
		for (const FunctionDecl& helper: _unit.helpers) {
			if (helper.body && callsBarrier(*helper.body)) {
				return;
			}
		}
		_masksGridSize = true;

		if (_globalInvocationVariable == InvalidId) {
			_builder.setSection(spirv::Section::TypesGlobals);
			const Id vectorType = _types.vector(ScalarKind::UInt, 3);
			_globalInvocationVariable = _builder.emitDeclTyped(spirv::OpVariable,
				_types.pointer(spirv::StorageClass::Input, vectorType),
				{ static_cast<uint32_t>(spirv::StorageClass::Input) });
			_builder.setSection(spirv::Section::Annotations);
			_builder.emit(spirv::OpDecorate, { _globalInvocationVariable,
				static_cast<uint32_t>(spirv::Decoration::BuiltIn),
				static_cast<uint32_t>(spirv::BuiltIn::GlobalInvocationId) });
			_interface.push_back(_globalInvocationVariable);
		}
	}

	// if (any(position_in_grid >= grid_size)) return; at the start of the kernel.
	void Emitter::emitGridMask() {
		if (!_masksGridSize) {
			return;
		}
		const Id vectorType = _types.vector(ScalarKind::UInt, 3);
		const Id position = _builder.emitTyped(spirv::OpLoad, vectorType, { _globalInvocationVariable });
		Id outside = InvalidId;
		for (uint32_t i = 0; i < 3; ++i) {
			const Id component = _builder.emitTyped(spirv::OpCompositeExtract, _uintType, { position, i });
			const Id past = _builder.emitTyped(spirv::OpUGreaterThanEqual, _boolType, { component, _gridSize[i] });
			outside = outside == InvalidId ? past
				: _builder.emitTyped(spirv::OpLogicalOr, _boolType, { outside, past });
		}
		const Id exitLabel = _builder.nextId();
		const Id bodyLabel = _builder.nextId();
		_builder.emit(spirv::OpSelectionMerge, { bodyLabel, kSelectionControlNone });
		terminate(spirv::OpBranchConditional, { outside, exitLabel, bodyLabel });
		beginBlock(exitLabel);
		terminate(spirv::OpReturn, { });
		beginBlock(bodyLabel);
	}

	// A kernel's `threadgroup T *p [[threadgroup(n)]]`. Metal sets the byte length at encode time
	// (setThreadgroupMemoryLength:atIndex:), Vulkan fixes Workgroup size at pipeline creation, so the
	// variable is a Workgroup array whose length is a specialization constant, set to the element
	// count the host computes from the byte length and the reflected element_size. The SpecId is
	// per argument, from 3 up, because SpecIds are per module and two kernels can take index n with
	// different element types; each entry point's reflection names its own.
	void Emitter::declareThreadgroupArgument(const Parameter& parameter, size_t index, uint32_t metalIndex) {
		const std::string what = "threadgroup argument \"" + parameter.name + "\"";
		if (parameter.isReference) {
			throw CompileError(what + " is a reference, which is not lowered yet; take a pointer");
		}
		Type element = parameter.type;
		element.isPointer = false;
		if (element.isAtomic) {
			throw CompileError(what + " points at an atomic, which is not lowered yet");
		}
		if (element.resource != ResourceKind::None || element.arrayLength
			|| (element.scalar == ScalarKind::Void && element.namedType.empty())) {
			throw CompileError(what + " has a pointee mslc cannot lay out in threadgroup memory; it takes a "
				"scalar, vector, matrix or struct");
		}
		const uint64_t elementSize = sizeofType(element, _unit);

		const Id elementType = declaredTypeOf(element);
		const Id count = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType, { 1u });
		if (_nextSpecId == kFunctionConstantSpecializationID) {
			throw CompileError(what + " is one more than mslc has specialization constants for");
		}
		const uint32_t specId = _nextSpecId++;
		_builder.emit(spirv::OpDecorate, { count, static_cast<uint32_t>(spirv::Decoration::SpecId), specId });
		const Id arrayType = _types.arrayOfLengthId(elementType, count);
		const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Workgroup, arrayType),
			{ static_cast<uint32_t>(spirv::StorageClass::Workgroup) });
		_interface.push_back(variable);

		// An array with no length of its own to check literal indexes against.
		Type msl = element;
		msl.arrayLength = UINT32_MAX;
		bindLocal(parameter.name, variable, arrayType, msl, spirv::StorageClass::Workgroup);

		addReflectionEntry("{ \"kind\": \"ThreadgroupMemory\", \"metal_index\": " + std::to_string(metalIndex)
			+ ", \"spec_id\": " + std::to_string(specId)
			+ ", \"element_size\": " + std::to_string(elementSize)
			+ ", \"param_index\": " + std::to_string(index)
			+ ", \"name\": \"" + parameter.name + "\" }");
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

	// A lod_clamp bound as a JSON number: the shortest decimal form of the float.
	static std::string lodNumber(float value) {
		char text[32];
		std::snprintf(text, sizeof text, "%.9g", static_cast<double>(value));
		return text;
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
			const bool texelBuffer = entry.parameter->type.resource == ResourceKind::TextureBuffer;
			if (entry.parameter->type.textureAccess == TextureAccess::Write) {
				_builder.emit(spirv::OpDecorate, { variable, static_cast<uint32_t>(spirv::Decoration::NonReadable) });
			} else if (entry.parameter->type.isStorageTexture() && entry.parameter->type.textureAccess == TextureAccess::Read) {
				_builder.emit(spirv::OpDecorate, { variable, static_cast<uint32_t>(spirv::Decoration::NonWritable) });
			}

			Binding bound;
			bound.id = variable;
			bound.isPointer = true;
			bound.pointeeType = imageType;
			bound.storageClass = spirv::StorageClass::UniformConstant;
			bound.pointeeMsl = entry.parameter->type;
			_bindings[entry.parameter->name] = bound;

			// A texel buffer is its own kind, so that a consumer that binds every "Texture" as an
			// image view skips it rather than binding the wrong descriptor type.
			addReflectionEntry(std::string("{ \"kind\": \"") + (texelBuffer ? "TextureBuffer" : "Texture")
				+ "\", \"metal_index\": "
				+ std::to_string(entry.metalIndex)
				+ ", \"descriptor\": " + descriptorJson(binding)
				+ (texelBuffer ? std::string(", \"descriptor_type\": \"")
					+ (entry.parameter->type.isStorageTexture() ? "StorageTexelBuffer" : "UniformTexelBuffer") + "\"" : std::string())
				+ ", \"texture_access\": \""
				+ (entry.parameter->type.textureAccess == TextureAccess::Write ? "Write"
					: entry.parameter->type.textureAccess == TextureAccess::Read ? "Read"
					: entry.parameter->type.textureAccess == TextureAccess::ReadWrite ? "ReadWrite" : "Sample") + "\""
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

	void Emitter::reserveThreadgroupLocals(const Statement& statement) {
		if (statement.declaration) { reserveThreadgroupLocal(*statement.declaration); }
		if (statement.forInitializer) { reserveThreadgroupLocal(*statement.forInitializer); }
		for (const StatementPtr& child: statement.children) {
			reserveThreadgroupLocals(*child);
		}
		for (const StatementPtr& child: statement.switchPreamble) {
			reserveThreadgroupLocals(*child);
		}
		for (const Statement* nested: { statement.thenBranch.get(), statement.elseBranch.get(),
			statement.forBody.get(), statement.whileBody.get() }) {
			if (nested) { reserveThreadgroupLocals(*nested); }
		}
		for (const SwitchCase& kase: statement.switchCases) {
			for (const StatementPtr& child: kase.body) {
				reserveThreadgroupLocals(*child);
			}
		}
	}

	// Only a plain value type is reserved; every other form is refused where it is declared.
	void Emitter::reserveThreadgroupLocal(const VariableDeclaration& declaration) {
		const Type& type = declaration.type;
		// An array is reserved here too. emitLocalArray looks the variable up in
		// this table when it runs, so the same shape serves both, and a
		// threadgroup array has to be in the OpEntryPoint interface just as a
		// threadgroup scalar does.
		if (type.addressSpace != AddressSpace::Threadgroup || type.isAuto || type.isPointer
			|| type.resource != ResourceKind::None || declaration.isReference
			|| declaration.initializer) {
			return;
		}
		const Id typeId = type.arrayLength ? arrayTypeOf(type) : declaredTypeOf(type);
		_threadgroupLocals[&declaration] = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Workgroup, typeId),
			{ static_cast<uint32_t>(spirv::StorageClass::Workgroup) });
		_interface.push_back(_threadgroupLocals[&declaration]);
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

	// What a call can reach, for the passes that run before emission. A member is chosen by its
	// receiver's struct, which is known only when the call is emitted, so every member of that name
	// is taken.
	std::vector<const FunctionDecl*> Emitter::functionsCalledBy(const Expression& call) {
		std::vector<const FunctionDecl*> found;
		const auto add = [&](const HelperFunction* helper) {
			if (helper->definition->body) { found.push_back(helper->definition); }
		};
		if (call.left->kind == ExpressionKind::Identifier) {
			for (const HelperFunction* helper: helpersCalled(call)) { add(helper); }
		} else if (call.left->kind == ExpressionKind::Member) {
			const std::string suffix = "::" + call.left->memberName;
			for (const auto& entry: _helpers) {
				const FunctionDecl& definition = *entry.second.definition;
				if (definition.name.size() > suffix.size()
					&& definition.name.compare(definition.name.size() - suffix.size(), suffix.size(), suffix) == 0) {
					add(&entry.second);
				}
			}
		}
		return found;
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

		// Every function the entry point reaches, with what it calls; one that names a sampler,
		// or calls one that does, is emitted per entry point (helperScope).
		std::map<const FunctionDecl*, std::vector<const FunctionDecl*>> callees;
		std::set<const FunctionDecl*> naming;
		std::vector<const FunctionDecl*> pending { _entryPoint };
		callees[_entryPoint];
		while (!pending.empty()) {
			const FunctionDecl* function = pending.back();
			pending.pop_back();

			std::vector<const Expression*> calls;
			collectCalls(*function->body, calls);
			for (const Expression* call: calls) {
				for (const FunctionDecl* callee: functionsCalledBy(*call)) {
					callees[function].push_back(callee);
					if (callees.try_emplace(callee).second) {
						pending.push_back(callee);
						if (scan(*callee)) {
							naming.insert(callee);
						}
					}
				}
			}
		}
		for (bool grew = true; grew;) {
			grew = false;
			for (const auto& [function, called]: callees) {
				if (function != _entryPoint && !naming.count(function)
					&& std::any_of(called.begin(), called.end(), [&](const FunctionDecl* callee) {
						return naming.count(callee) > 0; })) {
					naming.insert(function);
					grew = true;
				}
			}
		}
		_samplerDependent.insert(naming.begin(), naming.end());

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

	// "texture2d<float> b = a;", "sampler t = s;" and the auto forms: the new name is the same
	// descriptor as the one it was copied from, so nothing is declared and the reflection gains no
	// entry. Only a name is accepted on the right, and the name is never rebound (emitAssignment),
	// so there is no run-time choice among descriptors for the alias to stand for.
	void Emitter::bindResourceAlias(const VariableDeclaration& declaration, const Type* declared) {
		const Expression& source = *declaration.initializer;
		const std::string what = std::string("the ") + (declared && declared->resource == ResourceKind::Sampler
			? "sampler" : "texture or sampler") + " \"" + declaration.name + "\"";
		if (source.kind != ExpressionKind::Identifier) {
			throw CompileError(what + " has to be initialised from the name of a texture or sampler: a "
				"conditional, a call, a member or an element would pick the descriptor at run time, which "
				"mslc does not lower");
		}
		if (source.name == declaration.name) {
			throw CompileError(what + " is initialised from itself");
		}
		const Binding* named = findResourceBinding(source.name);
		if (!named || named->pointeeMsl.resource == ResourceKind::None) {
			throw CompileError(what + " is initialised from \"" + source.name + "\", which is not a texture or "
				"a sampler");
		}
		if (declared && !sameResourceType(named->pointeeMsl, *declared)) {
			throw CompileError(what + " is initialised from \"" + source.name + "\", which is not a "
				+ typeName(*declared));
		}
		_bindings[declaration.name] = *named;
	}

	// A half texture is sampled as a float image, a short one as an int image and a
	// ushort one as a uint image, and narrowed here, because the image type's sampled
	// type has 32 bits whatever the texture's component is.
	Id Emitter::texturePixels(const Binding& texture, Id sampled) {
		const ScalarKind element = texture.pointeeMsl.scalar;
		if (textureSampledKind(element) == element) {
			return sampled;
		}

		return convert(sampled, _builder.typeOf(sampled), _types.vector(element, 4));
	}

	// What sample and read return: a depth texture gives the first channel of the image's
	// four, one float (or half), and any other texture its four narrowed channels.
	Id Emitter::textureTexel(const Binding& texture, Id sampled) {
		if (!texture.pointeeMsl.isDepth) {
			return texturePixels(texture, sampled);
		}
		const Id depth = _builder.emitTyped(spirv::OpCompositeExtract, _types.scalar(ScalarKind::Float), { sampled, 0u });
		return texture.pointeeMsl.scalar == ScalarKind::Half
			? convert(depth, _types.scalar(ScalarKind::Float), _types.scalar(ScalarKind::Half)) : depth;
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

	static bool isArrayKind(ResourceKind kind) {
		return kind == ResourceKind::Texture2DArray || kind == ResourceKind::Texture1DArray
			|| kind == ResourceKind::TextureCubeArray;
	}
	static bool isCubeKind(ResourceKind kind) {
		return kind == ResourceKind::TextureCube || kind == ResourceKind::TextureCubeArray;
	}
	static bool isOneDimensional(ResourceKind kind) {
		return kind == ResourceKind::Texture1D || kind == ResourceKind::Texture1DArray;
	}
	static size_t layerArguments(ResourceKind kind);
	static uint32_t storageCoordinateWidth(ResourceKind kind);

	// write(value, coordinate [, layer]) in statement position, on an access::write or
	// access::read_write texture. The value is a vector of exactly the texture's own
	// element type, or any number, converted to that type and put in every channel
	// (measured with Apple's compiler: half4 into a float texture and uint4 into an int
	// one are refused, a float scalar into a half or int texture is not). The coordinate is a uint or ushort vector of the
	// texture's dimension; an array or a cube takes a uint2 and then the layer or
	// face. An explicit lod is not lowered.
	void Emitter::emitTextureWrite(const Expression& call, const Binding& texture) {
		const TextureAccess access = texture.pointeeMsl.textureAccess;
		if (access != TextureAccess::Write && access != TextureAccess::ReadWrite) {
			throw CompileError("the texture method \"write\" is not lowered yet on textures that are not "
				"access::write or access::read_write");
		}
		const ResourceKind kind = texture.pointeeMsl.resource;
		const size_t layers = layerArguments(kind);
		const uint32_t width = storageCoordinateWidth(kind);
		const std::string suffix = width == 1 ? std::string() : std::to_string(width);
		const ScalarKind element = texture.pointeeMsl.scalar;
		const ScalarKind sampled = textureSampledKind(element);
		const std::string takes = std::string("texture write takes exactly ") + (element == ScalarKind::Int ? "an " : "a ") + scalarKindName(element)
			+ "4 or a number and a uint" + suffix + " or ushort" + suffix + " coordinate"
			+ (layers == 2 ? " and a face and an array index" : layers == 1 ? (kind == ResourceKind::TextureCube ? " and a face" : " and a layer") : "");
		if (call.arguments.size() == 3 + layers) {
			throw CompileError("mip arguments are unsupported on access::" + std::string(textureAccessName(access))
				+ " textures; write takes only a value, a coordinate and a layer where the texture has one");
		}
		if (call.arguments.size() != 2 + layers) {
			throw CompileError(takes);
		}
		const Id value = emitExpression(*call.arguments[0]);
		Id coordinate = emitExpression(*call.arguments[1]);
		const Id coordinateType = _builder.typeOf(coordinate);
		const Id valueType = _builder.typeOf(value);
		const bool unsignedVector = _types.vectorWidth(coordinateType) == width && !_types.isFloat(coordinateType)
			&& !_types.isSignedInt(coordinateType) && (_types.bitWidth(coordinateType) == 32 || _types.bitWidth(coordinateType) == 16);
		const bool numeric = _types.vectorWidth(valueType) == 1;
		const bool ownVector = valueType == _types.vector(element, 4);
		if (!unsignedVector || !(numeric || ownVector)) {
			throw CompileError(takes);
		}
		coordinate = convert(coordinate, coordinateType, _types.vector(ScalarKind::UInt, width));
		coordinate = appendLayers(coordinate, kind, call.arguments, 2, "write");
		// The image's sampled type is the 32-bit type of the element, and a number is
		// narrowed to the element type before it is widened to it and put in every
		// channel, so a half or short texture stores the value rounded to its own type.
		const Id texelType = _types.vector(sampled, 4);
		Id texel;
		if (ownVector) {
			texel = convert(value, valueType, texelType);
		} else {
			texel = broadcast(convert(value, valueType, _types.scalar(element)), texelType);
		}
		const Id image = loadFrom(texture.id, texture.pointeeType);
		_builder.emit(spirv::OpImageWrite, { image, coordinate, texel });
	}

	Id Emitter::emitTextureCall(const Expression& call) {
		const Binding& texture = textureReceiver(call);
		const std::string& method = call.left->memberName;
		const ResourceKind kind = texture.pointeeMsl.resource;
		const TextureAccess access = texture.pointeeMsl.textureAccess;
		if (method == "write") {
			throw CompileError("texture write returns void and has no value");
		}
		if (texture.pointeeMsl.isMultisampled) {
			return emitMultisampledCall(call, texture);
		}
		if (kind == ResourceKind::TextureBuffer && method != "read" && method != "get_width") {
			throw CompileError("a texture_buffer has read, write and get_width only; \"" + method
				+ "\" is not a texture_buffer method");
		}
		const bool sized = method == "get_width" || method == "get_height"
			|| (method == "get_depth" && kind == ResourceKind::Texture3D)
			|| (method == "get_array_size" && isArrayKind(kind));
		if (access == TextureAccess::Write && !sized) {
			throw CompileError("write-only textures lower only the size queries and statement "
				"write(value, coordinate); \"" + method + "\" is not lowered yet");
		}
		if (access != TextureAccess::Sample && access != TextureAccess::Write && method != "read" && !sized) {
			throw CompileError(std::string("access::") + textureAccessName(access) + " textures lower only read(), "
				+ (access == TextureAccess::ReadWrite ? "statement write() and " : "") + "the size queries; \""
				+ method + "\" is not lowered yet");
		}
		if (method == "sample" && access == TextureAccess::Sample) {
			return emitTextureSample(call, texture);
		}
		if (method == "gather" && access == TextureAccess::Sample) {
			return emitTextureGather(call, texture);
		}
		if (method == "sample_compare" && access == TextureAccess::Sample && texture.pointeeMsl.isDepth) {
			return emitTextureSample(call, texture, true);
		}
		if (method == "gather_compare" && access == TextureAccess::Sample && texture.pointeeMsl.isDepth) {
			return emitTextureGather(call, texture, true);
		}
		if (method == "get_num_mip_levels" && access == TextureAccess::Sample) {
			if (!call.arguments.empty()) {
				throw CompileError("get_num_mip_levels takes no arguments");
			}
			if (!_imageQueryDeclared) {
				_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageQuery) });
				_imageQueryDeclared = true;
			}
			return _builder.emitTyped(spirv::OpImageQueryLevels, _uintType, { loadFrom(texture.id, texture.pointeeType) });
		}
		if (method == "get_height" && isOneDimensional(kind)) {
			throw CompileError(std::string("a ") + resourceKindName(kind) + " has no get_height; it is one texel high");
		}
		if (method == "get_width" || method == "get_height") {
			return emitTextureSize(call, texture,
				method == "get_width" ? SizeComponent::Width : SizeComponent::Height);
		}
		if (method == "read" && (access != TextureAccess::Sample || !isCubeKind(kind))) {
			return emitTextureRead(call, texture);
		}
		if (method == "get_depth" && kind == ResourceKind::Texture3D) {
			return emitTextureSize(call, texture, SizeComponent::Depth);
		}
		if (method == "get_array_size" && isArrayKind(kind)) {
			if (!call.arguments.empty()) {
				throw CompileError("get_array_size takes no arguments; the layer count has no mip level");
			}
			return emitTextureSize(call, texture, SizeComponent::ArraySize);
		}

		if (isCubeKind(kind)) {
			throw CompileError(std::string("the ") + resourceKindName(kind) + " method \"" + method
				+ "\" is not lowered yet; mslc lowers sample, gather, get_width and get_height on a cube "
				"and get_array_size on a cube array");
		}
		throw CompileError("the texture method \"" + method + "\" is not lowered yet; mslc lowers "
			"sample, read, get_width, get_height, get_depth on a texture3d and get_array_size on an "
			"array texture");
	}

	// The components of the coordinate argument of sample and read on an image of
	// this kind. A texture2d_array takes its layer as a separate argument, so its
	// coordinate is two wide and its size three; a cube's coordinate is a
	// direction, three wide, and its size two.
	static uint32_t textureCoordinateWidth(ResourceKind kind) {
		switch (kind) {
			case ResourceKind::Texture1D: return 1;
			case ResourceKind::Texture1DArray: return 1;
			case ResourceKind::TextureBuffer: return 1;
			case ResourceKind::TextureCubeArray: return 3;
			case ResourceKind::Texture2D: return 2;
			case ResourceKind::Texture2DArray: return 2;
			case ResourceKind::TextureCube: return 3;
			case ResourceKind::Texture3D: return 3;
			case ResourceKind::None:
			case ResourceKind::Sampler: break;
		}
		throw CompileError(std::string("\"") + resourceKindName(kind) + "\" has no texel coordinate");
	}

	// A texture1d has one level, and Apple's headers take its lod only as the
	// constant zero (read(uint, 0) is accepted, read(uint, 1) and a variable are not).
	static void requireTexture1DLevelZero(const Expression& lod, const std::string& callName) {
		if (lod.kind != ExpressionKind::IntLiteral || lod.intValue != 0) {
			throw CompileError("the lod of " + callName + " on a texture1d has to be the literal 0: "
				"a texture1d has one level");
		}
	}

	// A cube storage image keeps the cube view type, but is read and written by texel
	// and face: a uint2 and a face in Metal (a cube array adds the array index), a
	// three-component coordinate in SPIR-V.
	static size_t layerArguments(ResourceKind kind) {
		return kind == ResourceKind::TextureCubeArray ? 2
			: kind == ResourceKind::Texture2DArray || kind == ResourceKind::Texture1DArray || kind == ResourceKind::TextureCube ? 1 : 0;
	}

	static uint32_t storageCoordinateWidth(ResourceKind kind) {
		return isCubeKind(kind) ? 2 : textureCoordinateWidth(kind);
	}

	static uint32_t textureSizeWidth(ResourceKind kind) {
		return kind == ResourceKind::Texture1DArray ? 2
			: kind == ResourceKind::Texture2DArray || kind == ResourceKind::TextureCubeArray ? 3
			: kind == ResourceKind::TextureCube ? 2 : textureCoordinateWidth(kind);
	}

	// A texture2d_array coordinate of `component` with its layer appended as the
	// last component, which is where an arrayed image instruction takes it. The
	// layer is a uint in Metal, so an int wraps to uint before a sample sees it.
	Id Emitter::layerIndex(const Expression& layer, const std::string& callName, const char* what) {
		const Id value = emitExpression(layer);
		const Id valueType = _builder.typeOf(value);
		if (_types.vectorWidth(valueType) != 1 || _types.isFloat(valueType) || _types.bitWidth(valueType) < 8) {
			throw CompileError(std::string("the ") + what + " of " + callName + " has to be an integer");
		}

		return convert(value, valueType, _uintType);
	}

	Id Emitter::appendArrayLayer(Id coordinate, ScalarKind component, const Expression& layer,
		const std::string& callName) {
		const Id index = layerIndex(layer, callName);
		return _builder.emitTyped(spirv::OpCompositeConstruct,
			_types.vector(component, _types.vectorWidth(_builder.typeOf(coordinate)) + 1),
			{ coordinate, convert(index, _uintType, _types.scalar(component)) });
	}

	// The integer coordinate of read and write with the arguments that follow it, a
	// layer or a face, appended. A cube array takes a face and an array index, which
	// SPIR-V has as the one layer-face 6 * index + face.
	Id Emitter::appendLayers(Id coordinate, ResourceKind kind, const std::vector<ExpressionPtr>& arguments,
		size_t first, const std::string& callName) {
		const size_t layers = layerArguments(kind);
		if (layers == 0) {
			return coordinate;
		}
		if (layers == 1) {
			return appendArrayLayer(coordinate, ScalarKind::UInt, *arguments[first], callName);
		}
		const Id face = layerIndex(*arguments[first], callName);
		const Id index = layerIndex(*arguments[first + 1], callName);
		const Id layerFace = _builder.emitTyped(spirv::OpIAdd, _uintType,
			{ _builder.emitTyped(spirv::OpIMul, _uintType, { index, constantU32(6) }), face });
		return _builder.emitTyped(spirv::OpCompositeConstruct,
			_types.vector(ScalarKind::UInt, _types.vectorWidth(_builder.typeOf(coordinate)) + 1), { coordinate, layerFace });
	}

	static bool literalInteger(const Expression& expression, int32_t& value);

	// A texel offset written as an int vector of integer literals, as the OpConstantComposite
	// ConstOffset takes, or InvalidId for any other expression. Vulkan guarantees offsets of
	// -8 to 7 only, which is Metal's documented range, so a literal beyond it is refused.
	Id Emitter::literalOffset(const Expression& expression, uint32_t width) {
		const bool shape = expression.kind == ExpressionKind::Construct && expression.constructType
			&& expression.constructType->scalar == ScalarKind::Int && expression.constructType->vectorWidth == width
			&& (expression.arguments.size() == width || expression.arguments.size() == 1);
		if (!shape) {
			return InvalidId;
		}
		std::vector<uint32_t> parts;
		for (const ExpressionPtr& argument: expression.arguments) {
			int32_t value = 0;
			if (!literalInteger(*argument, value)) {
				return InvalidId;
			}
			if (value < -8 || value > 7) {
				throw CompileError("the texel offset " + std::to_string(value) + " is outside -8 to 7, the range Vulkan "
					"guarantees and Metal documents");
			}
			auto cached = _intConstants.find(value);
			if (cached == _intConstants.end()) {
				cached = _intConstants.emplace(value,
					_builder.emitDeclTyped(spirv::OpConstant, _intType, { static_cast<uint32_t>(value) })).first;
			}
			parts.push_back(cached->second);
		}
		parts.resize(width, parts.front());
		return _builder.emitDeclTyped(spirv::OpConstantComposite, _types.vector(ScalarKind::Int, width), parts);
	}

	// The sampler a sample or gather names as its first argument. A coord::pixel sampler
	// is only lowered on a texture2d (Vulkan allows unnormalized coordinates on
	// non-arrayed 1D and 2D views only).
	const Binding& Emitter::samplerArgument(const Expression& argument, ResourceKind kind, const std::string& callName) {
		if (argument.kind != ExpressionKind::Identifier) {
			throw CompileError("the first argument of " + callName + " has to name a sampler");
		}
		const Binding* samplerBinding = findResourceBinding(argument.name);
		if (!samplerBinding || samplerBinding->pointeeMsl.resource != ResourceKind::Sampler) {
			throw CompileError("\"" + argument.name + "\" is not a sampler, and " + callName + " takes one "
				"as its first argument");
		}
		if (kind != ResourceKind::Texture2D && samplerBinding->unnormalizedSampler) {
			throw CompileError(std::string("a ") + resourceKindName(kind) + " cannot be sampled with a "
				"coord::pixel sampler: mslc lowers unnormalized coordinates on texture2d only (Vulkan "
				"allows them on non-arrayed 1D and 2D image views). A sampler parameter is not checked, since its state is the host's");
		}
		return *samplerBinding;
	}

	// The float coordinate of a sample or gather, a number being broadcast, with the
	// array layer appended where the texture has one.
	Id Emitter::samplingCoordinate(const Expression& argument, ResourceKind kind, const Expression* layer,
		const std::string& callName) {
		const uint32_t width = textureCoordinateWidth(kind);
		const Id coordinateVector = _types.vector(ScalarKind::Float, width);
		Id coordinate = emitExpression(argument);
		const Id coordinateType = _builder.typeOf(coordinate);
		if (coordinateType != coordinateVector) {
			if (_types.vectorWidth(coordinateType) != 1 || _types.bitWidth(coordinateType) < 8) {
				throw CompileError("the coordinate of " + callName + " has to be a float"
					+ (width == 1 ? std::string() : std::to_string(width)) + " or a number");
			}
			coordinate = width == 1 ? convert(coordinate, coordinateType, coordinateVector)
				: broadcast(coordinate, coordinateVector);
		}
		if (layer) {
			coordinate = appendArrayLayer(coordinate, ScalarKind::Float, *layer, callName);
		}
		return coordinate;
	}

	// gather(sampler, coordinate [, array index] [, int2 offset] [, component::x..w]): the
	// four texels of a 2x2 footprint at one channel, as OpImageGather. The offset is a
	// texture2d and texture2d_array argument; a literal one is ConstOffset, any other an
	// Offset operand, which needs ImageGatherExtended. A coord::pixel sampler is refused.
	Id Emitter::emitTextureGather(const Expression& call, const Binding& texture, bool compare) {
		const auto& arguments = call.arguments;
		const ResourceKind kind = texture.pointeeMsl.resource;
		const bool arrayed = kind == ResourceKind::Texture2DArray || kind == ResourceKind::TextureCubeArray;
		if (kind != ResourceKind::Texture2D && kind != ResourceKind::TextureCube && !arrayed) {
			throw CompileError(std::string("gather is defined on texture2d, texture2d_array, texturecube and texturecube_array; ")
				+ resourceKindName(kind) + " has none");
		}
		const size_t fixed = (arrayed ? 3 : 2) + (compare ? 1 : 0);
		const std::string callName = compare ? "gather_compare" : "gather";
		if (arguments.size() < fixed) {
			throw CompileError(callName + " takes a sampler, a coordinate" + (arrayed ? ", an array index" : "")
				+ (compare ? ", a comparison value" : "") + ", and optionally an int2 offset" + (compare ? "" : " and a component"));
		}
		const Binding& sampler = samplerArgument(*arguments[0], kind, callName);
		if (sampler.unnormalizedSampler) {
			throw CompileError(callName + " through a coord::pixel sampler is not lowered: Vulkan allows no gather on an "
				"unnormalized sampler");
		}
		const Id coordinate = samplingCoordinate(*arguments[1], kind, arrayed ? arguments[2].get() : nullptr, callName);
		const Id reference = compare ? compareValue(*arguments[fixed - 1], callName) : InvalidId;

		Id offset = InvalidId;
		bool constantOffset = false;
		uint32_t component = 0;
		bool componentSeen = false;
		for (size_t i = fixed; i < arguments.size(); ++i) {
			const Expression& argument = *arguments[i];
			const std::string tail = argument.kind == ExpressionKind::Identifier
				? argument.name.substr(argument.name.rfind("::") == std::string::npos ? 0 : argument.name.rfind("::") + 2)
				: std::string();
			const bool isComponent = argument.kind == ExpressionKind::Identifier
				&& argument.name.find("component::") != std::string::npos;
			if (isComponent && (compare || texture.pointeeMsl.isDepth)) {
				throw CompileError(compare ? "gather_compare takes no component: it compares the depth channel"
					: "gather on a depth texture takes no component: it gathers the depth channel");
			}
			if (isComponent) {
				const auto found = std::string("xyzw").find(tail);
				// Apple takes the component only after the offset, where there is one.
				if (componentSeen || i != (isCubeKind(kind) ? fixed : fixed + 1)
					|| tail.size() != 1 || found == std::string::npos) {
					throw CompileError("gather takes one component::x, y, z or w, after the offset where the texture has one");
				}
				component = static_cast<uint32_t>(found);
				componentSeen = true;
			} else if (i == fixed && !isCubeKind(kind)) {
				offset = literalOffset(argument, 2);
				if (offset != InvalidId) {
					constantOffset = true;
				} else {
					offset = emitExpression(argument);
					if (_builder.typeOf(offset) != _types.vector(ScalarKind::Int, 2)) {
						throw CompileError("the offset of gather has to be an int2");
					}
				}
			} else {
				throw CompileError(std::string("gather takes ") + (isCubeKind(kind)
					? "a sampler, a coordinate and optionally a component; a cube has no offset"
					: "a sampler, a coordinate, an optional int2 offset and an optional component"));
			}
		}

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id loadedSampler = loadFrom(sampler.id, sampler.pointeeType);
		const Id combined = _builder.emitTyped(spirv::OpSampledImage,
			_types.sampledImage(kind, textureSampledKind(texture.pointeeMsl.scalar), texture.pointeeMsl.isDepth, false),
			{ image, loadedSampler });
		std::vector<uint32_t> words { combined, coordinate, compare ? reference : constantU32(component) };
		if (offset != InvalidId && constantOffset) {
			words.push_back(0x8);  // ConstOffset
			words.push_back(offset);
		} else if (offset != InvalidId) {
			if (!_gatherExtendedDeclared) {
				_gatherExtendedDeclared = true;
				_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageGatherExtended) });
			}
			words.push_back(kImageOperandsOffset);
			words.push_back(offset);
		}
		return texturePixels(texture, _builder.emitTyped(compare ? spirv::OpImageDrefGather : spirv::OpImageGather,
			_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4), words));
	}

	// sample(sampler, float2 coordinate [, level(lod)]), a float3 coordinate on a
	// texturecube or texture3d, or a float2 coordinate and a uint layer on a
	// texture2d_array. A fragment function takes
	// the level from the derivatives, which is OpImageSampleImplicitLod; a vertex
	// or kernel function has none, so it samples level 0 with an explicit lod,
	// and so does a sampler with pixel coordinates, which Vulkan does not allow
	// an implicit lod on.
	// An integer written as a literal, with an optional minus: the only constant a
	// texel offset can be here, since SPIR-V takes it as an OpConstant.
	static bool literalInteger(const Expression& expression, int32_t& value) {
		if (expression.kind == ExpressionKind::IntLiteral) {
			value = static_cast<int32_t>(expression.intValue);
			return expression.intValue <= INT32_MAX;
		}
		if (expression.kind == ExpressionKind::Unary && expression.unaryOperator == UnaryOperator::Negate
			&& expression.left && literalInteger(*expression.left, value)) {
			value = -value;
			return true;
		}
		return false;
	}

	// sample(sampler, coordinate [, array index] [, option] [, min_lod_clamp(m)] [, offset]).
	// The option is level(lod), bias(b), or gradient2d, gradient3d or gradientcube
	// for the texture's dimension. A fragment function with no option takes the level
	// from the derivatives, which is OpImageSampleImplicitLod; a vertex or kernel
	// function has none, so it samples level 0 with an explicit lod, and so does a
	// sampler with pixel coordinates, which Vulkan does not allow an implicit lod on.
	// bias and min_lod_clamp need those derivatives and so a fragment function, and an
	// offset has to be a vector of integer literals (ConstOffset).
	// The comparison value of sample_compare and gather_compare, a float.
	Id Emitter::compareValue(const Expression& argument, const std::string& callName) {
		const Id value = emitExpression(argument);
		const Id type = _builder.typeOf(value);
		if (_types.vectorWidth(type) != 1 || _types.bitWidth(type) < 8) {
			throw CompileError("the comparison value of " + callName + " has to be a number");
		}
		return convert(value, type, _types.scalar(ScalarKind::Float));
	}

	Id Emitter::emitTextureSample(const Expression& call, const Binding& texture, bool compare) {
		const auto& arguments = call.arguments;
		const ResourceKind kind = texture.pointeeMsl.resource;
		const bool arrayed = isArrayKind(kind);
		const size_t fixed = (arrayed ? 3 : 2) + (compare ? 1 : 0);
		if (arguments.size() < fixed || arguments.size() > fixed + 3) {
			throw CompileError(std::string("sample takes a sampler, a coordinate")
				+ (arrayed ? ", an array index" : "") + " and optionally an option, min_lod_clamp(m) and an offset; "
				"this call passes " + std::to_string(arguments.size()));
		}

		const std::string callName = compare ? "sample_compare" : "sample";
		const Binding& sampler = samplerArgument(*arguments[0], kind, callName);
		const Id coordinate = samplingCoordinate(*arguments[1], kind, arrayed ? arguments[2].get() : nullptr, callName);
		const Id reference = compare ? compareValue(*arguments[fixed - 1], callName) : InvalidId;

		if (isOneDimensional(kind) && arguments.size() > fixed) {
			throw CompileError(std::string("a ") + resourceKindName(kind) + " samples with a sampler and a coordinate only; "
				"a sample option such as level(lod) is not accepted on it");
		}

		const Id floatType = _types.scalar(ScalarKind::Float);
		const auto scalarArgument = [&](const Expression& option, const std::string& what) {
			if (option.arguments.size() != 1) {
				throw CompileError(what + " takes one argument");
			}
			const Id value = emitExpression(*option.arguments[0]);
			const Id valueType = _builder.typeOf(value);
			if (_types.vectorWidth(valueType) != 1 || _types.bitWidth(valueType) < 8) {
				throw CompileError(what + " takes a scalar number");
			}
			return convert(value, valueType, floatType);
		};
		const uint32_t gradientWidth = kind == ResourceKind::Texture3D || isCubeKind(kind) ? 3 : 2;
		const std::string gradientName = kind == ResourceKind::Texture3D ? "gradient3d"
			: isCubeKind(kind) ? "gradientcube" : "gradient2d";

		Id lod = InvalidId, bias = InvalidId, minLod = InvalidId, gradientX = InvalidId, gradientY = InvalidId, offset = InvalidId;
		int stage = 0;  // the option, then min_lod_clamp, then the offset, in this order
		for (size_t i = fixed; i < arguments.size(); ++i) {
			const Expression& option = *arguments[i];
			const bool isCall = option.kind == ExpressionKind::Call && option.left->kind == ExpressionKind::Identifier;
			const std::string name = isCall ? option.left->name : std::string();
			if (isCall && (name == "level" || name == "bias" || name == "gradient2d" || name == "gradient3d" || name == "gradientcube")) {
				if (stage > 0) {
					throw CompileError("the sample option " + name + "(...) has to come first, before min_lod_clamp and the offset");
				}
				stage = 1;
				if (name == "level") {
					lod = scalarArgument(option, "level");
				} else if (name == "bias") {
					bias = scalarArgument(option, "bias");
				} else {
					if (name != gradientName) {
						throw CompileError(std::string("a ") + resourceKindName(kind) + " takes " + gradientName
							+ "(...), not " + name + "(...)");
					}
					if (option.arguments.size() != 2) {
						throw CompileError(name + " takes two gradients");
					}
					const Id vectorType = _types.vector(ScalarKind::Float, gradientWidth);
					Id* targets[2] = { &gradientX, &gradientY };
					for (size_t g = 0; g < 2; ++g) {
						*targets[g] = emitExpression(*option.arguments[g]);
						if (_builder.typeOf(*targets[g]) != vectorType) {
							throw CompileError(name + " takes two float" + std::to_string(gradientWidth) + " gradients");
						}
					}
				}
			} else if (isCall && name == "min_lod_clamp") {
				if (stage > 1 || minLod != InvalidId) {
					throw CompileError("min_lod_clamp(...) comes after the option and before the offset, once");
				}
				stage = 2;
				minLod = scalarArgument(option, "min_lod_clamp");
			} else {
				if (stage > 2 || offset != InvalidId || isCubeKind(kind)) {
					throw CompileError(isCubeKind(kind) ? "a cube sample takes no offset"
						: "the sample argument after the offset is not lowered");
				}
				stage = 3;
				const uint32_t offsetWidth = kind == ResourceKind::Texture3D ? 3 : 2;
				offset = literalOffset(option, offsetWidth);
				if (offset == InvalidId) {
					throw CompileError("the offset of sample has to be written as an int" + std::to_string(offsetWidth)
						+ " of integer literals: Vulkan takes a constant texel offset");
				}
			}
		}

		const bool derivatives = bias != InvalidId || (lod == InvalidId && gradientX == InvalidId);
		if (sampler.unnormalizedSampler && compare) {
			throw CompileError("sample_compare through a coord::pixel sampler is not lowered: Vulkan allows no depth "
				"comparison on an unnormalized sampler");
		}
		if (sampler.unnormalizedSampler && (bias != InvalidId || minLod != InvalidId || gradientX != InvalidId || offset != InvalidId)) {
			throw CompileError("a coord::pixel sampler takes level(lod) or no option only: Vulkan fixes Lod 0 on it");
		}
		if (minLod != InvalidId && lod != InvalidId) {
			throw CompileError("min_lod_clamp(...) with level(...) is not lowered: Vulkan takes MinLod with an "
				"implicit lod or gradients only");
		}
		const bool library = _entryPoint == &_libraryScope;
		if (derivatives && !sampler.unnormalizedSampler && (bias != InvalidId || minLod != InvalidId)) {
			if (library) {
				throw StageError("bias or min_lod_clamp in a library helper: they need derivatives, which exist only in a "
					"fragment function, and a library does not know its caller's stage");
			}
			if (_entryPoint->stage != Stage::Fragment) {
				throw CompileError("bias and min_lod_clamp are lowered in a fragment function only: they act on the "
					"level the derivatives give, which a vertex or kernel function does not have");
			}
		}

		// Vulkan requires Lod 0 through an unnormalized sampler, whatever level() says.
		if (sampler.unnormalizedSampler) {
			lod = _builder.emitDeclTyped(spirv::OpConstant, floatType, { 0u });
		}

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id loadedSampler = loadFrom(sampler.id, sampler.pointeeType);
		const Id combined = _builder.emitTyped(spirv::OpSampledImage,
			_types.sampledImage(texture.pointeeMsl.resource, textureSampledKind(texture.pointeeMsl.scalar), texture.pointeeMsl.isDepth, false),
			{ image, loadedSampler });

		const Id float4 = _types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4);
		const bool noOption = lod == InvalidId && bias == InvalidId && gradientX == InvalidId;
		if (noOption && minLod == InvalidId && library && !sampler.unnormalizedSampler) {
			throw StageError("a sample with no explicit level in a library helper: whether it is implicit "
				"depends on the stage of the entry point that calls it, which a library does not know; "
				"pass level(...)");
		}
		const bool implicit = (noOption && _entryPoint->stage == Stage::Fragment && !sampler.unnormalizedSampler)
			|| bias != InvalidId || (minLod != InvalidId && gradientX == InvalidId);
		if (noOption && !implicit) {
			lod = _builder.emitDeclTyped(spirv::OpConstant, floatType, { 0u });
		}
		uint32_t mask = 0;
		std::vector<uint32_t> words { combined, coordinate };
		if (compare) {
			words.push_back(reference);
		}
		const size_t maskAt = words.size();
		words.push_back(0);
		if (bias != InvalidId) {
			mask |= 0x1;
			words.push_back(bias);
		}
		if (lod != InvalidId) {
			mask |= kImageOperandsLod;
			words.push_back(lod);
		}
		if (gradientX != InvalidId) {
			mask |= 0x4;  // Grad
			words.push_back(gradientX);
			words.push_back(gradientY);
		}
		if (offset != InvalidId) {
			mask |= 0x8;  // ConstOffset
			words.push_back(offset);
		}
		if (minLod != InvalidId) {
			mask |= 0x80;  // MinLod
			words.push_back(minLod);
			if (!_minLodDeclared) {
				_minLodDeclared = true;
				_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::MinLod) });
			}
		}
		if (mask == 0) {
			words.pop_back();
		} else {
			words[maskAt] = mask;
		}
		if (compare) {
			return _builder.emitTyped(implicit ? spirv::OpImageSampleDrefImplicitLod : spirv::OpImageSampleDrefExplicitLod,
				_types.scalar(ScalarKind::Float), words);
		}
		const Id sampled = _builder.emitTyped(implicit ? spirv::OpImageSampleImplicitLod : spirv::OpImageSampleExplicitLod,
			float4, words);
		return textureTexel(texture, sampled);
	}

	// read(uint2 coordinate [, lod]), a uint3 coordinate on a texture3d, or a uint2
	// coordinate and a uint layer on a texture2d_array: a texel by integer
	// coordinate, with no sampler. access::read storage images take no lod. Metal
	// takes uint and ushort vectors and rejects int and float ones.
	Id Emitter::emitTextureRead(const Expression& call, const Binding& texture) {
		const auto& arguments = call.arguments;
		const ResourceKind kind = texture.pointeeMsl.resource;
		const bool storage = texture.pointeeMsl.isStorageTexture();
		if (kind == ResourceKind::TextureBuffer && arguments.size() != 1) {
			throw CompileError("read on a texture_buffer takes one coordinate: a texture_buffer has no mip levels");
		}
		const size_t layers = storage ? layerArguments(kind) : isArrayKind(kind) ? 1 : 0;
		const bool arrayed = layers > 0;
		const size_t fixed = 1 + layers;
		if (storage && arguments.size() == fixed + 1) {
			throw CompileError(std::string("mip arguments are unsupported on access::")
				+ textureAccessName(texture.pointeeMsl.textureAccess) + " textures; read takes only a coordinate");
		}
		if (arguments.size() < fixed || arguments.size() > fixed + 1) {
			throw CompileError(std::string("read takes a coordinate")
				+ (arrayed ? ", an array index" : "") + " and optionally a lod; this call passes "
				+ std::to_string(arguments.size()));
		}

		const uint32_t width = storage ? storageCoordinateWidth(kind) : textureCoordinateWidth(kind);
		Id coordinate = emitExpression(*arguments[0]);
		const Id coordinateType = _builder.typeOf(coordinate);
		const bool unsignedVector = _types.vectorWidth(coordinateType) == width
			&& !_types.isFloat(coordinateType) && !_types.isSignedInt(coordinateType)
			&& (_types.bitWidth(coordinateType) == 32 || _types.bitWidth(coordinateType) == 16);
		if (!unsignedVector) {
			const std::string suffix = width == 1 ? std::string() : std::to_string(width);
			throw CompileError("the coordinate of read has to be a uint" + suffix + " or a ushort" + suffix);
		}
		coordinate = convert(coordinate, coordinateType, _types.vector(ScalarKind::UInt, width));
		coordinate = appendLayers(coordinate, storage ? kind : arrayed ? ResourceKind::Texture2DArray : ResourceKind::Texture2D,
			arguments, 1, "read");

		if (storage) {
			const Id storageImage = loadFrom(texture.id, texture.pointeeType);
			return texturePixels(texture, _builder.emitTyped(spirv::OpImageRead,
				_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4), { storageImage, coordinate }));
		}

		if (kind == ResourceKind::TextureBuffer) {
			// Dim Buffer takes no Lod operand.
			return texturePixels(texture, _builder.emitTyped(spirv::OpImageFetch,
				_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4),
				{ loadFrom(texture.id, texture.pointeeType), coordinate }));
		}
		if (isOneDimensional(kind) && arguments.size() == fixed + 1) {
			requireTexture1DLevelZero(*arguments[fixed], "read");
		}
		Id lod = arguments.size() == fixed + 1 && !isOneDimensional(kind)
			? emitLod(*arguments[fixed], "read") : constantU32(0);

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id fetched = _builder.emitTyped(spirv::OpImageFetch,
			_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4), { image, coordinate, kImageOperandsLod, lod });
		return textureTexel(texture, fetched);
	}

	// The methods of a multisampled texture: read(coordinate [, array index], sample) is
	// OpImageFetch with the Sample operand and no lod, get_num_samples is
	// OpImageQuerySamples, and the size queries take no argument.
	Id Emitter::emitMultisampledCall(const Expression& call, const Binding& texture) {
		const std::string& method = call.left->memberName;
		const auto& arguments = call.arguments;
		const ResourceKind kind = texture.pointeeMsl.resource;
		const bool arrayed = isArrayKind(kind);
		if (method == "get_width" || method == "get_height") {
			return emitTextureSize(call, texture, method == "get_width" ? SizeComponent::Width : SizeComponent::Height);
		}
		if (method == "get_array_size" && arrayed) {
			return emitTextureSize(call, texture, SizeComponent::ArraySize);
		}
		if (method == "get_num_samples") {
			if (!arguments.empty()) {
				throw CompileError("get_num_samples takes no arguments");
			}
			if (!_imageQueryDeclared) {
				_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageQuery) });
				_imageQueryDeclared = true;
			}
			return _builder.emitTyped(spirv::OpImageQuerySamples, _uintType, { loadFrom(texture.id, texture.pointeeType) });
		}
		if (method != "read") {
			throw CompileError("the " + typeName(texture.pointeeMsl) + " method \"" + method + "\" is not lowered yet; mslc lowers "
				"read, get_num_samples, get_width, get_height and get_array_size on a multisampled texture");
		}
		const size_t fixed = arrayed ? 3 : 2;
		if (arguments.size() != fixed) {
			throw CompileError(std::string("read on a multisampled texture takes a coordinate")
				+ (arrayed ? ", an array index" : "") + " and a sample index");
		}
		Id coordinate = emitExpression(*arguments[0]);
		const Id coordinateType = _builder.typeOf(coordinate);
		const bool unsignedPair = _types.vectorWidth(coordinateType) == 2 && !_types.isFloat(coordinateType)
			&& !_types.isSignedInt(coordinateType) && (_types.bitWidth(coordinateType) == 32 || _types.bitWidth(coordinateType) == 16);
		if (!unsignedPair) {
			throw CompileError("the coordinate of read has to be a uint2 or a ushort2");
		}
		coordinate = convert(coordinate, coordinateType, _types.vector(ScalarKind::UInt, 2));
		if (arrayed) {
			coordinate = appendArrayLayer(coordinate, ScalarKind::UInt, *arguments[1], "read");
		}
		const Id sampleIndex = layerIndex(*arguments[fixed - 1], "read", "sample index");
		const Id image = loadFrom(texture.id, texture.pointeeType);
		return textureTexel(texture, _builder.emitTyped(spirv::OpImageFetch,
			_types.vector(textureSampledKind(texture.pointeeMsl.scalar), 4),
			{ image, coordinate, 0x40 /* Sample */, sampleIndex }));
	}

	// get_width(), get_height() and get_depth() or get_array_size() (component 0,
	// 1 and 2 of the size), of level 0 or of the lod given.
	Id Emitter::emitTextureSize(const Expression& call, const Binding& texture, SizeComponent component) {
		const std::string& name = call.left->memberName;
		const bool buffer = texture.pointeeMsl.resource == ResourceKind::TextureBuffer;
		const bool storage = texture.pointeeMsl.textureAccess != TextureAccess::Sample || texture.pointeeMsl.isMultisampled;
		if (storage && !call.arguments.empty()) {
			throw CompileError(buffer ? name + " takes no arguments: a texture_buffer has no mip levels"
				: texture.pointeeMsl.isMultisampled
				? name + " takes no arguments: a multisampled texture has no mip levels"
				: std::string("mip arguments are unsupported on access::")
					+ textureAccessName(texture.pointeeMsl.textureAccess) + " textures; " + name + " takes no arguments");
		}
		if (call.arguments.size() > 1) {
			throw CompileError(name + " takes an optional lod; this call passes "
				+ std::to_string(call.arguments.size()));
		}

		const bool oneLevel = isOneDimensional(texture.pointeeMsl.resource);
		if (oneLevel && !call.arguments.empty()) {
			requireTexture1DLevelZero(*call.arguments[0], name);
		}
		const Id lod = storage ? InvalidId
			: call.arguments.empty() || oneLevel ? constantU32(0) : emitLod(*call.arguments[0], name);

		if (!_imageQueryDeclared) {
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::ImageQuery) });
			_imageQueryDeclared = true;
		}

		const Id image = loadFrom(texture.id, texture.pointeeType);
		const Id size = _builder.emitTyped(storage ? spirv::OpImageQuerySize : spirv::OpImageQuerySizeLod,
			_types.vector(ScalarKind::UInt, textureSizeWidth(texture.pointeeMsl.resource)),
			storage ? std::vector<uint32_t>{ image } : std::vector<uint32_t>{ image, lod });
		if (texture.pointeeMsl.resource == ResourceKind::Texture1D || buffer) {
			return size;
		}
		const uint32_t index = component == SizeComponent::ArraySize && texture.pointeeMsl.resource == ResourceKind::Texture1DArray
			? 1u : static_cast<uint32_t>(component);
		return _builder.emitTyped(spirv::OpCompositeExtract, _uintType, { size, index });
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
			Id base = _builder.emitTyped(spirv::OpLoad, memberType, { memberAddress });
			const auto matrixType = _bufferMatrixTypes.find(static_cast<uint32_t>(k));
			if (matrixType != _bufferMatrixTypes.end()) {
				// The base of the reference is the matrix inside its wrapper struct.
				base = _builder.emitTyped(spirv::OpAccessChain,
					_types.pointer(spirv::StorageClass::PhysicalStorageBuffer, matrixType->second),
					{ base, constantU32(0) });
			}
			_bufferBases.emplace(static_cast<uint32_t>(k), base);
			const auto boolType = _bufferBoolTypes.find(static_cast<uint32_t>(k));
			if (boolType != _bufferBoolTypes.end()) {
				boolAddress(base, boolType->second);
			}
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

		// A bool crosses from a vertex function to a fragment function as an unsigned
		// integer that is 0 or 1; Apple refuses it as a colour output, and a bool
		// vertex attribute has no layout here.
		const bool crossesStages = (_entryPoint->stage == Stage::Vertex && storageClass == spirv::StorageClass::Output)
			|| (_entryPoint->stage == Stage::Fragment && storageClass == spirv::StorageClass::Input);
		const bool carriedBool = type.scalar == ScalarKind::Bool && crossesStages;
		// A char or a short is stored in 32 bits across the boundary, as a half is in a
		// float; Apple accepts them there and refuses 64-bit integers.
		const bool narrowInteger = crossesStages && (type.scalar == ScalarKind::Char || type.scalar == ScalarKind::UChar
			|| type.scalar == ScalarKind::Short || type.scalar == ScalarKind::UShort);

		const uint32_t bits = scalarBitWidth(type.scalar);
		if (type.isPointer || type.arrayLength || !type.namedType.empty() || type.isMatrix()
			|| (type.scalar == ScalarKind::Bool && !carriedBool)
			|| (bits != 32 && type.scalar != ScalarKind::Half && !carriedBool && !narrowInteger)) {
			throw CompileError(what + " is a " + typeName(type) + ", which mslc does not pass "
				"between stages yet; it passes a scalar or a vector of 32-bit components or of half");
		}

		Type carried = type;
		if (carried.scalar == ScalarKind::Half) {
			carried.scalar = ScalarKind::Float;
		}
		if (carriedBool) {
			carried.scalar = ScalarKind::UInt;
		}
		if (narrowInteger) {
			carried.scalar = type.scalar == ScalarKind::Char || type.scalar == ScalarKind::Short
				? ScalarKind::Int : ScalarKind::UInt;
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
			if (field.attributes.position || field.attributes.colorIndex) {
				locations.emplace_back(std::nullopt);
			} else if (const auto named = field.attributes.userName
				? locnLocation(*field.attributes.userName) : std::nullopt) {
				locations.emplace_back(*named);
			} else if (field.attributes.pointSize) {
				locations.emplace_back(std::nullopt);
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
		bool hasPointSize = false;

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
			if (field.attributes.colorIndex) {
				if (!isInput) {
					throw CompileError(what + " is [[color(n)]], which is not valid on a vertex output; it names a "
						"fragment output's colour attachment");
				}
				if (field.attributes.position || field.attributes.pointSize || field.attributes.userName
					|| field.attributes.attributeIndex || field.attributes.interpolation != Interpolation::None) {
					throw CompileError(what + " is [[color(n)]], which cannot also carry another attribute");
				}
				variables.push_back(declareFramebufferFetch(field.type, *field.attributes.colorIndex, field.name, what));
				continue;
			}
			const Interpolation interpolation = field.attributes.interpolation;
			if (field.attributes.position && interpolation != Interpolation::None) {
				throw CompileError(what + " is [[position]], which cannot also have an interpolation attribute");
			}
			if (field.attributes.pointSize) {
				if (isInput) {
					throw CompileError(what + " has [[point_size]] on a fragment input, which mslc does not lower yet");
				}
				if (field.attributes.position) {
					throw CompileError(what + " has an incompatible point_size attribute combination");
				}
				if (field.attributes.userName || interpolation != Interpolation::None) {
					throw CompileError(what + " combines point_size with user or interpolation, which mslc does not lower");
				}
				if (hasPointSize) {
					throw CompileError("struct \"" + decl.name + "\" has more than one [[point_size]] field");
				}
				const Type& type = field.type;
				if (type.scalar != ScalarKind::Float || !type.isScalar() || type.isPointer
					|| type.arrayLength || !type.namedType.empty()) {
					throw CompileError(what + " has [[point_size]], which has to be a scalar float");
				}
				hasPointSize = true;
			}

			if (!isInput && !field.type.enumName.empty()) {
				throw CompileError(what + " has the enum type " + field.type.enumName + ", which Apple's "
					"compiler does not allow in a vertex function's returned struct");
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

				if (isInput) {
					if (_hasFragCoordInput) {
						throw CompileError(what + " is [[position]], which collides with this "
							"function's other fragment position input");
					}
					_hasFragCoordInput = true;
				}

				hasPosition = true;
				_builder.emit(spirv::OpDecorate, { variable.variable,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(isInput ? spirv::BuiltIn::FragCoord : spirv::BuiltIn::Position) });
			} else if (field.attributes.pointSize) {
				_builder.emit(spirv::OpDecorate, { variable.variable,
					static_cast<uint32_t>(spirv::Decoration::BuiltIn),
					static_cast<uint32_t>(spirv::BuiltIn::PointSize) });
			} else {
				_builder.emit(spirv::OpDecorate, { variable.variable,
					static_cast<uint32_t>(spirv::Decoration::Location), *locations[index] });

				// The pairing of a vertex output with a fragment input is by name (see stageLocations), and
				// the two functions are usually compiled apart, so the host that links them needs each one's
				// names, types and Locations.
				addReflectionEntry(std::string("{ \"kind\": \"") + (isInput ? "StageInput" : "StageOutput")
					+ "\", \"location\": " + std::to_string(*locations[index]) + ", \"name\": \""
					+ field.attributes.userName.value_or(field.name) + "\", \"type\": \"" + typeName(field.type) + "\" }");

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
			if (field.attributes.pointSize) {
				throw CompileError(what + " has [[point_size]], which a vertex function's [[stage_in]] "
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
				if (field.attributes.colorIndex || field.attributes.position || field.attributes.pointSize
					|| field.attributes.sampleMask) {
					throw CompileError(what + " has an incompatible depth attribute combination");
				}
				if (field.attributes.attributeIndex) {
					throw CompileError(what + " combines depth with an attribute index, which mslc does not lower");
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
				if (field.attributes.colorIndex || field.attributes.position || field.attributes.pointSize) {
					throw CompileError(what + " has an incompatible sample_mask attribute combination");
				}
				if (field.attributes.attributeIndex) {
					throw CompileError(what + " combines sample_mask with an attribute index, which mslc does not lower");
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
			if (field.attributes.pointSize) {
				throw CompileError(what + " has [[point_size]], which is only valid on a vertex output");
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

			Type carried = field.type;
			if ((carried.scalar == ScalarKind::Short || carried.scalar == ScalarKind::UShort)
				&& carried.vectorWidth <= 4 && !carried.isPointer && !carried.arrayLength && !carried.isPacked) {
				carried.scalar = carried.scalar == ScalarKind::Short ? ScalarKind::Int : ScalarKind::UInt;
			}
			StageVariable variable = declareStageVariable(carried, spirv::StorageClass::Output, what);
			variable.valueType = declaredTypeOf(field.type);
			variables.push_back(variable);
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

	// Framebuffer fetch, [[color(n)]] on a fragment input field or parameter: Vulkan reads the
	// colour attachment a previous subpass wrote only through an input attachment, so the value
	// is an OpImageRead of one whose InputAttachmentIndex is n. The descriptor is bound by
	// declareAttachments, after every texture and sampler binding of the entry point.
	StageVariable Emitter::declareFramebufferFetch(const Type& type, uint32_t index, const std::string& name,
		const std::string& what) {
		constexpr uint32_t maxColorAttachments = 8;
		if (_entryPoint->stage != Stage::Fragment) {
			throw CompileError(what + " is [[color(n)]], which only a fragment function reads");
		}
		const bool sampled = type.scalar == ScalarKind::Float || type.scalar == ScalarKind::Half
			|| type.scalar == ScalarKind::Int || type.scalar == ScalarKind::UInt;
		if (type.isPointer || type.arrayLength || !type.namedType.empty() || type.isMatrix()
			|| type.isPacked || type.vectorWidth != 4 || !sampled) {
			throw CompileError(what + " is [[color(n)]] with type " + typeName(type) + ", and mslc "
				"reads a colour attachment only as a float4, half4, int4 or uint4");
		}
		if (index >= maxColorAttachments) {
			throw CompileError(what + " is [[color(" + std::to_string(index) + ")]], and Apple's "
				"compiler allows indices 0 to 7");
		}
		if (!_attachmentIndices.insert(index).second) {
			throw CompileError(what + " reuses [[color(" + std::to_string(index) + ")]], which "
				"another input of this function already reads");
		}

		// Apple pairs the fetched colour with the output of the same index and refuses a pair of
		// different types, so a half4 read cannot be written back as a float4.
		const Type& returned = _entryPoint->returnType;
		const Type* output = nullptr;
		if (const StructDecl* decl = structValue(returned)) {
			for (const StructField& field: decl->fields) {
				if (field.attributes.colorIndex == index) {
					output = &field.type;
				}
			}
		} else if (index == 0 && returned.scalar != ScalarKind::Void && !returned.isPointer) {
			output = &returned;
		}
		if (output && (output->scalar != type.scalar || output->vectorWidth != type.vectorWidth)) {
			throw CompileError(what + ": 'color' attachment " + std::to_string(index) + " has different "
				"types on input (" + typeName(type) + ") and output (" + typeName(*output) + ")");
		}

		Type carried = type;
		if (carried.scalar == ScalarKind::Half) {
			carried.scalar = ScalarKind::Float;
		}
		StageVariable fetch;
		fetch.attachment = index;
		fetch.sampled = carried.scalar;
		fetch.name = name;
		fetch.valueType = declaredTypeOf(type);
		fetch.interfaceType = declaredTypeOf(carried);
		return fetch;
	}

	// One input attachment descriptor per fetched colour, after the textures and samplers and
	// in colour order, in the fragment function's set. A consumer binds the attachment's
	// view there and maps pInputAttachments[input_attachment_index] to the colour attachment.
	void Emitter::declareAttachments() {
		std::vector<StageVariable*> fetches;
		for (StageVariable& input: _stageInputs) {
			if (input.attachment) {
				fetches.push_back(&input);
			}
		}
		for (AttachmentParameter& fetched: _attachmentParameters) {
			fetches.push_back(&fetched.variable);
		}
		std::sort(fetches.begin(), fetches.end(), [](const StageVariable* a, const StageVariable* b) {
			return *a->attachment < *b->attachment;
		});

		for (StageVariable* fetch: fetches) {
			const uint32_t binding = _nextBinding++;
			const Id imageType = _types.subpassImage(fetch->sampled);
			fetch->variable = declareDescriptorVariable(imageType, binding);
			_builder.emit(spirv::OpDecorate, { fetch->variable,
				static_cast<uint32_t>(spirv::Decoration::InputAttachmentIndex), *fetch->attachment });
			addReflectionEntry("{ \"kind\": \"InputAttachment\", \"metal_index\": "
				+ std::to_string(*fetch->attachment) + ", \"descriptor\": " + descriptorJson(binding)
				+ ", \"input_attachment_index\": " + std::to_string(*fetch->attachment)
				+ ", \"scalar\": \"" + std::string(fetch->sampled == ScalarKind::Float ? "float"
					: fetch->sampled == ScalarKind::Int ? "int" : "uint") + "\""
				+ ", \"name\": \"" + fetch->name + "\" }");
		}
	}

	Id Emitter::readAttachment(const StageVariable& attachment) {
		const Id imageType = _types.subpassImage(attachment.sampled);
		const Id image = loadFrom(attachment.variable, imageType);
		const Id origin = _types.zero(_types.vector(ScalarKind::Int, 2));
		const Id texel = _builder.emitTyped(spirv::OpImageRead, attachment.interfaceType, { image, origin });
		return convert(texel, attachment.interfaceType, attachment.valueType);
	}

	// The [[stage_in]] parameter becomes a local holding the struct, filled from
	// the Input variables at the top of the function, so a field of it is read
	// the way a field of any local is.
	void Emitter::loadStageInputs() {
		for (const AttachmentParameter& fetched: _attachmentParameters) {
			const Id declared = fetched.variable.valueType;
			const Id local = _builder.emitDeclTyped(spirv::OpVariable,
				_types.pointer(spirv::StorageClass::Function, declared),
				{ static_cast<uint32_t>(spirv::StorageClass::Function) });
			_builder.emit(spirv::OpStore, { local, readAttachment(fetched.variable) });
			bindLocal(fetched.parameter->name, local, declared, fetched.parameter->type);
		}

		if (!_stageIn) {
			return;
		}

		const Id structType = declaredTypeOf(_stageIn->type);
		std::vector<uint32_t> fields;
		for (const StageVariable& input: _stageInputs) {
			if (input.attachment) {
				fields.push_back(readAttachment(input));
				continue;
			}
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

	// [[threads_per_threadgroup]] is the workgroup size, which the module holds as
	// three spec constants for indium to fill in. indium refuses a partial
	// threadgroup, so the size is the same for every group. The parameter lives
	// in the function's own storage, as a by-value parameter does.
	void Emitter::loadThreadsPerThreadgroup() {
		if (!_threadsPerThreadgroup) {
			return;
		}

		const Type& type = _threadsPerThreadgroup->type;
		const Id declared = declaredTypeOf(type);
		const Id component = _types.scalar(type.scalar);
		const uint32_t width = type.isScalar() ? 1 : type.vectorWidth;
		std::vector<uint32_t> parts;
		for (uint32_t i = 0; i < width; ++i) {
			parts.push_back(type.scalar == ScalarKind::UInt ? _localSize[i]
				: _builder.emitTyped(spirv::OpUConvert, component, { _localSize[i] }));
		}

		const Id value = width == 1 ? parts[0]
			: _builder.emitTyped(spirv::OpCompositeConstruct, declared, parts);
		const Id local = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Function, declared),
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });
		_builder.emit(spirv::OpStore, { local, value });

		bindLocal(_threadsPerThreadgroup->name, local, declared, type);
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
			emitVoidReturnValue(*statement.expression, "\"" + _entryPoint->name + "\"");
			terminate(spirv::OpReturn, { });
			return;
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
			const Id returned = convertImplicit(value, declared);
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
					if (!field.attributes.position && !field.attributes.pointSize && !field.attributes.colorIndex) {
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

		// No vertex pairs by name+type; if one covers every input name, report the
		// first type mismatch rather than staying silent.
		for (const FunctionDecl* fragment: _entryPoints) {
			if (fragment->stage != Stage::Fragment) {
				continue;
			}

			for (const Parameter& parameter: fragment->parameters) {
				if (!parameter.attributes.stageIn) {
					continue;
				}

				const auto inputs = interfaceFields(parameter.type.namedType);

				bool fullyPaired = false;
				for (const FunctionDecl* vertex: _entryPoints) {
					if (vertex->stage != Stage::Vertex || vertex->returnType.namedType.empty()) {
						continue;
					}
					const auto outputs = interfaceFields(vertex->returnType.namedType);
					const bool pairs = std::all_of(inputs.begin(), inputs.end(), [&](const auto& input) {
						return std::find(outputs.begin(), outputs.end(), input) != outputs.end();
					});
					if (pairs) {
						fullyPaired = true;
						break;
					}
				}
				if (fullyPaired) {
					continue;
				}

				for (const FunctionDecl* vertex: _entryPoints) {
					if (vertex->stage != Stage::Vertex || vertex->returnType.namedType.empty()) {
						continue;
					}
					const auto outputs = interfaceFields(vertex->returnType.namedType);
					const bool namesCovered = std::all_of(inputs.begin(), inputs.end(), [&](const auto& input) {
						return std::find_if(outputs.begin(), outputs.end(), [&](const auto& output) {
							return output.name == input.name;
						}) != outputs.end();
					});
					if (!namesCovered) {
						continue;
					}

					for (const auto& input: inputs) {
						const auto candidate = std::find_if(outputs.begin(), outputs.end(), [&](const auto& output) {
							return output.name == input.name;
						});
						// namesCovered already guarantees candidate != outputs.end().
						if (candidate->type != input.type) {
							throw CompileError("vertex function \"" + vertex->name + "\" writes \""
								+ input.name + "\" as " + candidate->type + " and fragment function \""
								+ fragment->name + "\" reads it as " + input.type + "; mslc requires a "
								"stage-crossing field to have one type on both sides, since Apple rejects "
								"the mismatch when the pipeline is created");
						}
					}
					break;
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

		if (isVectorComponent(place)) {
			// A reference to a component of a vector has no storage to name. Apple refuses a non-const
			// one, and binds a const one to a temporary copy of the component.
			if (!declaration.type.isConst) {
				throw CompileError("the reference \"" + declaration.name + "\" is bound to a component of a "
					"vector, which a non-const reference cannot name (Apple refuses it too); copy the "
					"component or bind a const reference");
			}
			if (declaration.type.addressSpace != AddressSpace::Thread) {
				throw CompileError("the reference \"" + declaration.name + "\" to a component of a vector has to be "
					"spelled thread: Apple refuses a reference with no address space, and one in another space "
					"cannot bind the thread copy");
			}
			const Id value = emitExpression(place);
			const Type referenced = declaration.type.isAuto
				? deduceAutoType(declaration.type, _builder.typeOf(value), declaration.name) : declaration.type;
			requireSameReferent(declaration, referenced, _builder.typeOf(value));
			const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
				_types.pointer(spirv::StorageClass::Function, _builder.typeOf(value)),
				{ static_cast<uint32_t>(spirv::StorageClass::Function) });
			_builder.emit(spirv::OpStore, { variable, value });
			bindLocal(declaration.name, variable, _builder.typeOf(value), referenced);
			return;
		}
		if (bindsTemporary(place) && !declaration.type.isConst
			&& declaration.type.addressSpace != AddressSpace::Constant) {
			throw CompileError("the reference \"" + declaration.name + "\" is bound to a member or element of "
				"a temporary, which a non-const reference cannot bind (Apple refuses it too)");
		}
		rejectPlaceThroughConditional(place, "binding a reference to");

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
			const Type referenced = declaration.type.isAuto
				? deduceAutoType(declaration.type, alias.pointeeType, declaration.name) : declaration.type;
			requireSameReferent(declaration, referenced, alias.pointeeType);
			_bindings[declaration.name] = alias;
			return;
		}

		const Id address = emitPlaceAddress(place);
		const Id pointee = valueTypeAt(address);
		const auto storageClass = _types.storageClassOf(_builder.typeOf(address));
		if (pointee == InvalidId || !storageClass) {
			throw CompileError("cannot determine what the reference \"" + declaration.name + "\" names");
		}
		const Type referenced = declaration.type.isAuto
			? deduceAutoType(declaration.type, pointee, declaration.name) : declaration.type;
		requireSameReferent(declaration, referenced, pointee);

		Binding binding;
		binding.id = address;
		binding.isPointer = true;
		binding.pointeeType = pointee;
		binding.storageClass = *storageClass;
		binding.pointeeMsl = referenced;
		binding.structType = referenced.namedType.empty()
			? nullptr : _unit.findStruct(referenced.namedType);
		binding.readOnly = constReference || rootReadOnly;
		binding.isAddress = true;
		_bindings[declaration.name] = binding;
	}

	// A reference binds without a conversion: a different type would be a
	// temporary, which is a value and not the place.
	void Emitter::requireSameReferent(const VariableDeclaration& declaration, const Type& type, Id referent) {
		if (!type.namedType.empty()) {
			TypeTable::StructForm have;
			if (!_types.structForm(referent, have) || have.name != type.namedType) {
				throw CompileError("the reference \"" + declaration.name + "\" is not the type of the place "
					"it is bound to");
			}
			return;
		}
		if (referent != declaredTypeOf(type)) {
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
		const auto local = _bindings.find(expression.name);
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
				return std::string();
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
					return std::nullopt;
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
				if (expression.left->kind != ExpressionKind::Identifier) {
					return std::nullopt;
				}
				const Type* returnedType = returningType(expression, [](const Type& type) {
					return type.isPointer || type.arrayLength ? std::string("*") : type.enumName;
				});
				if (!returnedType) {
					return std::nullopt;
				}
				const Type& returned = *returnedType;
				return returned.isPointer || returned.arrayLength ? std::nullopt
					: std::optional<std::string>(returned.enumName);
			}
			case ExpressionKind::Index: {
				if (const Type* pointer = pointerMemberType(*expression.left)) {
					return pointer->enumName;
				}
				if (expression.left->kind != ExpressionKind::Identifier) return std::nullopt;
				const auto local = _bindings.find(expression.left->name);
				if (local == _bindings.end()) return std::nullopt;
				return local->second.pointeeMsl.enumName;
			}
			default:
				return std::nullopt;
		}
	}

	// The error for a store of something that is not the target's enum. An
	// enumerator of an anonymous enum can not be told from the target's here, so it passes.
	void Emitter::requireEnumValue(const std::string& target, const Expression& value,
		const std::string& what) {
		const auto given = enumOf(value);
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
		// array<T, N> a = {{1, 2, 3}}: the outer braces are the struct's and the inner ones the
		// array member's, where {1, 2, 3} reaches the member by brace elision.
		if (type.isStdArray && braced && entries.size() == 1 && entries[0].fieldName.empty()
			&& entries[0].value->kind == ExpressionKind::InitList) {
			size_t inner = 0;
			return emitArrayInit(type, entries[0].value->elements, inner, true, leaves);
		}
		Type element = type;
		element.dropOuterDimension();
		const bool rows = element.arrayLength.has_value();
		const uint32_t length = *type.arrayLength;

		std::vector<uint32_t> parts;
		while (parts.size() < length && position < entries.size()) {
			const InitializerElement& entry = entries[position];
			if (!entry.fieldName.empty()) {
				throw CompileError("a field designator requires a struct initializer");
			}
			if (rows && entry.value->kind != ExpressionKind::InitList) {
				parts.push_back(emitArrayInit(element, entries, position, false, leaves));
				continue;
			}
			++position;
			if (rows) {
				size_t inner = 0;
				parts.push_back(emitArrayInit(element, entry.value->elements, inner, true, leaves));
			} else {
				parts.push_back(leaves.element(element, entry.value.get()));
			}
		}
		if (braced && position < entries.size()) {
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
		collectIdentifiers(initializer, names);
		if (names.count(declaration.name)) {
			throw CompileError("variable \"" + declaration.name + "\" declared with deduced type 'auto' "
				"cannot appear in its own initializer");
		}

		if (initializer.kind == ExpressionKind::Identifier) {
			const Binding* named = findResourceBinding(initializer.name);
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
		if (!storageOnly && !type.enumName.empty() && declaration.initializer
			&& declaration.initializer->kind != ExpressionKind::InitList) {
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
			while (expression->kind == ExpressionKind::InitList && expression->elements.size() == 1) {
				expression = expression->elements.front().value.get();
			}
			FoldedConstant folded;
			folded.scalar = type.scalar;
			const bool empty = expression->kind == ExpressionKind::InitList && expression->elements.empty();
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
			if (pointerMemberType(left)) {
				throw CompileError("the pointer member \"" + left.memberName + "\" is only lowered "
					"when indexed or dereferenced, as in s.p[i] or *s.p");
			}
			Id fieldType = InvalidId;
			address = emitMemberAddress(left, fieldType);
		} else if (left.kind == ExpressionKind::Identifier) {
			// A local names a Function-storage pointer, so the variable itself is
			// the address to store to. A buffer parameter is a descriptor and a
			// plain value is loaded, so only this case takes the binding.
			const auto it = _bindings.find(left.name);
			if (it != _bindings.end() && it->second.pointeeMsl.isAtomic) {
				throw CompileError("the atomic \"" + left.name + "\" cannot be used as an ordinary place; "
					"use the atomic functions");
			}
			if (it != _bindings.end() && it->second.isPointer
				&& it->second.bufferPointeeType == InvalidId
				&& (it->second.storageClass == spirv::StorageClass::Function
					|| it->second.storageClass == spirv::StorageClass::Workgroup
					|| it->second.storageClass == spirv::StorageClass::PhysicalStorageBuffer)) {
				address = it->second.id;
			} else if (it != _bindings.end() && it->second.bufferPointeeType != InvalidId
				&& !it->second.isBuffer) {
				// A reference parameter is the one value its buffer holds.
				address = bufferBase(it->second);
			} else {
				address = emitExpression(left);
			}
		} else if (left.kind == ExpressionKind::Conditional) {
			throw CompileError("assigning to a conditional expression is not supported; "
				"assign inside an if instead");
		} else if (left.kind == ExpressionKind::Comma) {
			throw CompileError("assigning to a comma expression is not lowered yet; "
				"assign to its right operand on its own");
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
		if (readOnlyRoot(*root)) {
			throw CompileError("assigning to " + quoted + " of \"" + (root->kind == ExpressionKind::Member
				? root->memberName : root->name) + "\", which is const or in constant memory");
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
		// (void)e and static_cast<void>(e): e is evaluated for its effects and its
		// value dropped, so an assignment or a call in it still happens.
		if (isVoidCast(expression)) {
			// (void)param; is the idiom for an unused parameter: reading a texture, a
			// sampler or a buffer name has no value to evaluate, so naming a parameter is
			// all there is to do. An undeclared name is still reported.
			const Expression& operand = *expression.arguments.front();
			if (operand.kind == ExpressionKind::Identifier && findResourceBinding(operand.name)) {
				return;
			}
			emitExpressionStatement(operand);
			return;
		}

		if (expression.kind == ExpressionKind::Comma) {
			emitExpressionStatement(*expression.left);
			emitExpressionStatement(*expression.right);
			return;
		}

		// c ? (void)a : (void)b has no value, so it runs as the if it stands for.
		if (expression.kind == ExpressionKind::Conditional && isVoidValued(expression)) {
			const Id condition = asCondition(emitExpression(*expression.left));
			const Id thenLabel = _builder.nextId();
			const Id elseLabel = _builder.nextId();
			const Id mergeLabel = _builder.nextId();
			_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
			terminate(spirv::OpBranchConditional, { condition, thenLabel, elseLabel });

			const ControlDepthScope depth(_controlDepth);
			beginBlock(thenLabel);
			emitExpressionStatement(*expression.arguments.at(0));
			branchUnlessTerminated(mergeLabel);
			beginBlock(elseLabel);
			emitExpressionStatement(*expression.arguments.at(1));
			branchUnlessTerminated(mergeLabel);
			beginBlock(mergeLabel);
			return;
		}

		// ++x and x++ as a statement are x += 1; their value is not lowered.
		if (expression.kind == ExpressionKind::Unary && isStep(expression.unaryOperator)) {
			if (const auto stepped = enumOf(*expression.left); stepped && !stepped->empty()) {
				throw CompileError("++ and -- do not apply to the enum " + *stepped
					+ "; Apple does not convert an integer back to it");
			}
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
			std::vector<Id> prepared;
			if (HelperFunction* helper = findHelper(expression, receiver, prepared)) {
				emitHelperCall(expression, *helper, receiver, prepared);
				return;
			}
			const std::string name = expression.left->kind == ExpressionKind::Identifier
				? builtinCalleeName(expression.left->name) : std::string();
			if (name == "atomic_store_explicit") {
				emitAtomicCall(expression);
				return;
			}
			if (expression.left->kind == ExpressionKind::Identifier
				&& (name == "threadgroup_barrier" || name == "simdgroup_barrier")) {
				emitBarrierCall(expression);
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
	// accesses, since constness and storage follow the chain. A pointer member,
	// as s.p in s.p[i], ends the chain: what it points at is not part of s.
	const Expression& Emitter::storeRoot(const Expression& target) {
		const Expression* root = &target;
		while ((root->kind == ExpressionKind::Member && !pointerMemberType(*root))
			|| root->kind == ExpressionKind::Index) {
			root = root->left.get();
		}
		return *root;
	}

	// Whether a store through a place's root is refused: a const or constant
	// variable, or a pointer member to const or constant memory.
	bool Emitter::readOnlyRoot(const Expression& root) {
		if (const Type* pointer = pointerMemberType(root)) {
			return pointer->isConst || pointer->addressSpace == AddressSpace::Constant;
		}
		const auto binding = root.kind == ExpressionKind::Identifier ? _bindings.find(root.name) : _bindings.end();
		return binding != _bindings.end() && binding->second.readOnly;
	}

	// A store to something that is not a variable of the entry point: a literal,
	// which is what an enumerator is by now, or a file-scope constant.
	void Emitter::requireStorable(const Expression& target) {
		const Expression& root = storeRoot(target);
		if (root.kind == ExpressionKind::IntLiteral || root.kind == ExpressionKind::FloatLiteral
			|| root.kind == ExpressionKind::BoolLiteral) {
			throw CompileError("cannot assign to a literal or an enumerator constant");
		}
		if (isTemporaryPart(target)) {
			throw CompileError("expression is not assignable");
		}
		rejectPlaceThroughConditional(target, "assigning to");
		if (root.kind == ExpressionKind::Identifier && _bindings.find(root.name) == _bindings.end()
			&& (_constants.count(root.name) || functionConstantNamed(root.name))) {
			throw CompileError("cannot store to \"" + root.name + "\", a constant declared at file scope");
		}
	}

	void Emitter::emitAssignment(const Expression& left, const Expression& right,
		std::optional<BinaryOperator> compound) {
		requireStorable(left);
		// A struct prvalue takes the trivial assignment: the right side is evaluated first, as
		// Apple does, then the left, and the result is dropped.
		if (left.kind == ExpressionKind::InitList || left.kind == ExpressionKind::Call) {
			const StructDecl* target = structOf(left);
			if (!target) {
				throw CompileError("expression is not assignable");
			}
			if (compound) {
				throw CompileError("no viable overloaded compound assignment for a " + target->name + " temporary");
			}
			const Id value = emitExpression(right);
			const std::string* source = _types.structNameOf(_builder.typeOf(value), nullptr);
			if (!source || *source != target->name) {
				throw CompileError("no viable overloaded '=' for a " + target->name + " temporary");
			}
			emitExpression(left);
			return;
		}
		if (left.kind == ExpressionKind::Identifier) {
			const auto named = _bindings.find(left.name);
			if (named != _bindings.end() && named->second.pointeeMsl.resource != ResourceKind::None
				&& !named->second.pointeeMsl.arrayLength) {
				throw CompileError("\"" + left.name + "\" is a " + typeName(named->second.pointeeMsl)
					+ ", which cannot be assigned: mslc binds a texture or sampler name to one descriptor "
					"and does not lower choosing another at run time");
			}
			if (named != _bindings.end() && named->second.pointeeMsl.arrayLength && !named->second.isBuffer
				&& !(named->second.pointeeMsl.isStdArray && isLocalStorage(named->second.storageClass))) {
				const bool unsized = *named->second.pointeeMsl.arrayLength == UINT32_MAX;
				throw CompileError(unsized ? "the threadgroup argument \"" + left.name + "\" is not assignable; assign through it"
					: "array type " + typeName(named->second.pointeeMsl) + " is not assignable");
			}
		}
		if (const auto target = enumOf(left); target && !target->empty() && *target != "<anonymous>") {
			if (compound) {
				throw CompileError("a compound assignment does not apply to the enum " + *target
					+ "; Apple does not convert the integer result back to it");
			}
			requireEnumValue(*target, right, "the target of the assignment");
		}

		if (left.kind == ExpressionKind::Member
			&& !structOf(*left.left)) {
			emitSwizzleStore(left, right, compound);
			return;
		}

		const Expression* root = &storeRoot(left);
		if (readOnlyRoot(*root)) {
			throw CompileError("cannot store through \"" + (root->kind == ExpressionKind::Member
				? root->memberName : root->name) + "\", which is in the "
				"constant address space or declared const");
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
		} else if (right.kind == ExpressionKind::InitList && !right.constructType) {
			const std::optional<Type> target = placeType(left);
			if (!target || !target->isStdArray || !target->arrayLength || target->isPointer) {
				throw CompileError("assigning a braced list is lowered to an array<T, N> only");
			}
			value = emitInitListValue(*target, right);
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
							if (child->declaration->type.isConstexpr && !child->declaration->type.isAuto) {
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
			// A do-while moves the condition into the continue block.
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

				const bool isDoWhile = !isFor && statement.isDoWhile;
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
				const bool splitsCondition = condition && !isDoWhile && containsBranchingOperator(*condition);
				Id conditionValue = InvalidId;
				if (condition && !splitsCondition && !isDoWhile) {
					conditionValue = asCondition(emitExpression(*condition));
				}
				_builder.emit(spirv::OpLoopMerge, { mergeLabel, continueLabel, kLoopControlNone });
				if (splitsCondition) {
					const Id conditionLabel = _builder.nextId();
					terminate(spirv::OpBranch, { conditionLabel });
					beginBlock(conditionLabel);
					conditionValue = asCondition(emitExpression(*condition));
				}
				if (isDoWhile) {
					terminate(spirv::OpBranch, { bodyLabel });
				} else if (condition) {
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
				if (isDoWhile) {
					const Id again = asCondition(emitExpression(*condition));
					terminate(spirv::OpBranchConditional, { again, headerLabel, mergeLabel });
				} else {
					terminate(spirv::OpBranch, { headerLabel });
				}

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
				// A helper is emitted once for each entry point that calls it, so the
				// stage here is the caller's: OpKill in it drops that caller's fragment,
				// and a helper called from a vertex or kernel function is refused below.
				if (_entryPoint == &_libraryScope) {
					throw StageError("discard_fragment() drops a fragment, which only a fragment function has");
				}
				if (_entryPoint->stage != Stage::Fragment) {
					throw StageError(std::string("discard_fragment() is not allowed within a ")
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

		// mslc models Metal's safe math (__METAL_FAST_MATH__ is 0), so signed zeros,
		// infinities and NaNs must survive. Without this a NIR driver folds x * 0 and
		// x + (-x). The capability is core from SPIR-V 1.4; Vulkan needs
		// shaderSignedZeroInfNanPreserveFloat32. The 16-bit mode is added in run()
		// once it is known whether the module uses half.
		if (!_floatControlsDeclared) {
			_builder.emitNoResult(spirv::OpCapability,
				{ static_cast<uint32_t>(spirv::Capability::SignedZeroInfNanPreserve) });
			_floatControlsDeclared = true;
		}
		_builder.setSection(spirv::Section::ExecutionModes);
		_builder.emitNoResult(spirv::OpExecutionMode, { _entryPointId,
			static_cast<uint32_t>(spirv::ExecutionMode::SignedZeroInfNanPreserve), 32u });
		_entryPointIds.push_back(_entryPointId);
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
			// picked.
			//
			// Created once and reused, so a module with several kernels still has
			// exactly three SpecId decorations and every LocalSizeId names the same
			// three ids. Emitting a second set is not a validation error, which is
			// why this has to be checked here and not by the suite alone.
			if (_localSize[0] == InvalidId) {
				const Id x = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType,
					{ _options.localSizeX });
				const Id y = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType,
					{ _options.localSizeY });
				const Id z = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType,
					{ _options.localSizeZ });
				_localSize[0] = x;
				_localSize[1] = y;
				_localSize[2] = z;
				_builder.emit(spirv::OpDecorate, { x,
					static_cast<uint32_t>(spirv::Decoration::SpecId), 0u });
				_builder.emit(spirv::OpDecorate, { y,
					static_cast<uint32_t>(spirv::Decoration::SpecId), 1u });
				_builder.emit(spirv::OpDecorate, { z,
					static_cast<uint32_t>(spirv::Decoration::SpecId), 2u });
			}

			// The grid size of the threads a masked kernel keeps, also one set for the module. Their default is
			// the largest value, which keeps every thread, so a dispatch that does not know about them is not
			// changed. The ids are far from the ones a function constant takes (indium numbers those from 3).
			if (_masksGridSize && _gridSize[0] == InvalidId) {
				for (uint32_t i = 0; i < 3; ++i) {
					_gridSize[i] = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType, { 0xFFFFFFFFu });
					_builder.emit(spirv::OpDecorate, { _gridSize[i],
						static_cast<uint32_t>(spirv::Decoration::SpecId), kGridSizeSpecializationID + i });
				}
			}

			// OpExecutionModeId, not OpExecutionMode: a mode whose extra operands
			// are ids has its own instruction, and OpExecutionMode rejects id
			// operands outright.
			//
			// The section is set again here because the decorations above moved it
			// to the annotations, and an execution mode emitted from there lands
			// in the wrong section and is ignored.
			_builder.setSection(spirv::Section::ExecutionModes);
			_builder.emit(spirv::OpExecutionModeId, { _entryPointId,
				static_cast<uint32_t>(spirv::ExecutionMode::LocalSizeId),
				_localSize[0], _localSize[1], _localSize[2] });
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
		loadThreadsPerThreadgroup();
		emitGridMask();
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

	void Emitter::linkageAttributes(Id function, const std::string& name, uint32_t type) {
		std::vector<uint32_t> operands { function, static_cast<uint32_t>(spirv::Decoration::LinkageAttributes) };
		spirv::Builder::appendString(operands, name);
		operands.push_back(type);
		_builder.emit(spirv::OpDecorate, operands);
	}

	// Declarations come before every definition in a module's function section, so the
	// helpers this unit calls and does not define are emitted before the first body.
	void Emitter::emitImports() {
		_entryPoint = &_libraryScope;
		// What the unit emits as a body: its entry points, what it exports, and what
		// those call. A helper nothing reaches is not emitted, so it imports nothing.
		std::set<const FunctionDecl*> seen;
		std::vector<const FunctionDecl*> pending = _entryPoints;
		for (const FunctionDecl& helper: _unit.helpers) {
			// A member is reached through a receiver this scan cannot resolve, so every
			// member body counts as emitted: an import it needs must come before any body.
			if (isExported(helper) || (helper.body && isMemberName(helper.name))) {
				pending.push_back(&helper);
			}
		}
		std::vector<HelperFunction*> imports;
		while (!pending.empty()) {
			const FunctionDecl* function = pending.back();
			pending.pop_back();
			if (!seen.insert(function).second) {
				continue;
			}
			std::vector<const Expression*> calls;
			collectCalls(*function->body, calls);
			for (const Expression* call: calls) {
				if (call->left->kind != ExpressionKind::Identifier) {
					continue;
				}
				const std::vector<HelperFunction*> called = helpersCalled(*call);
				for (HelperFunction* found: called) {
					if (found->definition->body) {
						pending.push_back(found->definition);
					} else if (called.size() > 1) {
						throw CompileError("\"" + found->definition->name + "(" + parameterSignature(*found->definition)
							+ ")\" is declared but defined elsewhere, and a call to it is chosen among overloads; "
							"importing one overload of a name is not lowered yet");
					} else {
						imports.push_back(found);
					}
				}
			}
		}
		for (HelperFunction* entry: imports) {
			if (std::any_of(entry->definition->parameters.begin(), entry->definition->parameters.end(),
				[](const Parameter& parameter) { return parameter.type.isPointer; })) {
				throw CompileError("\"" + entry->definition->name + "\" is declared but defined elsewhere, and "
					"takes a pointer parameter; a thread pointer is lowered per caller, so it cannot be a "
					"link-time symbol");
			}
			helperFunctionId(*entry);
		}
		for (size_t i = 0; i < _helperQueue.size(); ++i) {
			emitHelper(*_helperQueue[i]);
		}
		_helperQueue.clear();
	}

	bool Emitter::isExported(const FunctionDecl& helper) const {
		return helper.body && _options.exportOnly && _options.exportOnly->count(helper.name);
	}

	// A Linkage object exports each helper it defines that lowers without a stage, whether
	// or not anything in the unit calls it: another unit's call is what makes it live.
	void Emitter::emitLibraryHelpers() {
		_entryPoint = &_libraryScope;
		for (const FunctionDecl& helper: _unit.helpers) {
			if (!isExported(helper)) {
				continue;
			}
			if (std::any_of(helper.parameters.begin(), helper.parameters.end(),
				[](const Parameter& parameter) { return parameter.type.isPointer; })) {
				throw CompileError("helper \"" + helper.name + "\" takes a pointer parameter, which is lowered "
					"per caller, so it cannot be exported from a library");
			}
			try {
				linkageAttributes(helperFunctionId(_helpers.at(helperKey(helper, _overloadedNames))),
					linkageName(helper), kLinkageExport);
				for (size_t i = 0; i < _helperQueue.size(); ++i) {
					emitHelper(*_helperQueue[i]);
				}
				_helperQueue.clear();
			} catch (const StageError& error) {
				throw CompileError("helper \"" + helper.name + "\" cannot be exported from a library, which does "
					"not know the stage of its caller: " + error.what());
			}
		}
	}

	// Emits the whole module: the capabilities, the memory model and the
	// file-scope globals once, then every entry point. Returns the reflection
	// document.
	std::string Emitter::run() {
		_builder.setSection(spirv::Section::Capabilities);
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Shader) });
		if (_options.linkageObject) {
			_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Linkage) });
		}

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

		_overloadedNames = overloadedNames(_unit);
		// Where each name is first declared, so a call that finds it among several names can tell
		// whether it is visible yet.
		std::map<std::string, size_t> firstDeclared;
		for (const FunctionDecl& helper: _unit.helpers) {
			firstDeclared.emplace(helper.name, helper.order);
		}
		for (const FunctionDecl& helper: _unit.helpers) {
			if (!_overloadedNames.count(helper.name)) {
				if (helper.body) {
					HelperFunction& function = _helpers[helper.name];
					function.definition = &helper;
					function.firstOrder = firstDeclared.at(helper.name);
				}
				continue;
			}
			// An overloaded name has every signature registered, declared or defined, so a
			// call can be refused for the right one.
			const auto entry = _helpers.try_emplace(helperKey(helper, _overloadedNames));
			HelperFunction& function = entry.first->second;
			if (entry.second) {
				function.definition = &helper;
				function.firstOrder = helper.order;
				_overloads[helper.name].push_back(&function);
			} else if (helper.body && !function.definition->body) {
				function.definition = &helper;
			}
		}
		// A name the unit only declares stands for the declaration. Not a standard library
		// redeclaration (a call to it is the builtin), and not a member, whose call
		// resolves through its struct.
		// (In an executable too: a call resolved among several names must see it, and then refuse it.)
		{
			for (const FunctionDecl& helper: _unit.helpers) {
				if (helper.body || helper.name.compare(0, stdlibPrefix.size(), stdlibPrefix) == 0
					|| isMemberName(helper.name) || _overloadedNames.count(helper.name)) {
					continue;
				}
				const auto entry = _helpers.try_emplace(helper.name);
				if (!entry.first->second.definition) {
					entry.first->second.definition = &helper;
					entry.first->second.firstOrder = firstDeclared.at(helper.name);
				}
			}
		}

		if (_options.linkageObject) {
			emitImports();
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
			bindFileScopeArrays(_entryPoint->globalsBefore);
			_bufferMembers.clear();
			_bufferBoolTypes.clear();
			_bufferMatrixTypes.clear();
			_bufferBases.clear();
			_interface.clear();
			for (const FileScopeArray& array: _fileScopeArrays) {
				_interface.push_back(array.variable);
			}
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
			_threadsPerThreadgroup = nullptr;
			_globalInvocationVariable = InvalidId;
			_masksGridSize = false;
			_stageInputs.clear();
			_attachmentParameters.clear();
			_attachmentIndices.clear();
			_hasFragCoordInput = false;
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
			if (_masksGridSize) {
				document += "\t\t\t\"grid_size_spec_ids\": [" + std::to_string(kGridSizeSpecializationID) + ", "
					+ std::to_string(kGridSizeSpecializationID + 1) + ", "
					+ std::to_string(kGridSizeSpecializationID + 2) + "],\n";
			}
			const std::string functionConstants = functionConstantsJson();
			if (!functionConstants.empty()) {
				document += "\t\t\t\"function_constants\": [" + functionConstants + "\n\t\t\t],\n";
			}
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
					+ ", \"compare_function\": \"" + samplerCompareName(state.compare) + "\", \"anisotropy\": "
					+ std::to_string(state.anisotropy) + ", \"border_color\": \"" + samplerBorderName(state.border)
					+ "\", \"lod_min\": " + lodNumber(state.lodMin) + ", \"lod_max\": " + lodNumber(state.lodMax)
					+ " }";
			}
			document += _embeddedSamplers.empty() ? "]\n" : "\n\t\t\t]\n";
			document += k + 1 < _entryPoints.size() ? "\t\t},\n" : "\t\t}\n";
		}

		document += "\t]\n}\n";

		if (_options.linkageObject) {
			emitLibraryHelpers();
		}

		// After the library helpers, which can be the only code that declares half.
		if (_types.declaresHalf()) {
			_builder.setSection(spirv::Section::ExecutionModes);
			for (const Id entry: _entryPointIds) {
				_builder.emitNoResult(spirv::OpExecutionMode, { entry,
					static_cast<uint32_t>(spirv::ExecutionMode::SignedZeroInfNanPreserve), 16u });
			}
		}

		// After every entry point, so a diagnostic about one function's own
		// interface comes before one about how two of them pair up.
		checkStageInterfacesAgree();

		return document;
	}

}

std::string emitModule(spirv::Builder& builder, const TranslationUnit& unit,
	const std::vector<const FunctionDecl*>& entryPoints, const ModuleOptions& options) {
	validateHelpers(unit, options.linkageObject);
	ModuleOptions chosen = options;
	if (options.linkageObject) {
		// Each helper is tried alone in a scratch module. One that needs a stage cannot be
		// exported; with an entry point it stays local to that stage's copies, and without
		// one nothing could ever call it, so it is an error.
		std::set<std::string> exportable;
		const std::vector<const FunctionDecl*> none;
		for (const FunctionDecl& helper: unit.helpers) {
			if (!helper.body || helper.isLocal) {
				continue;
			}
			ModuleOptions probe = options;
			probe.exportOnly = std::set<std::string> { helper.name };
			spirv::Builder scratch;
			TypeTable scratchTypes(scratch, unit);
			Emitter attempt(scratch, unit, none, probe, scratchTypes);
			try {
				attempt.run();
				exportable.insert(helper.name);
			} catch (const CompileError&) {
				if (entryPoints.empty()) {
					throw;
				}
			}
		}
		chosen.exportOnly = std::move(exportable);
	}
	TypeTable types(builder, unit);
	Emitter emitter(builder, unit, entryPoints, chosen, types);
	return emitter.run();
}

}
