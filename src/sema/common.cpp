#include "sema/emitter.h"

namespace mslc {

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

constexpr MathBuiltin kMathBuiltins[] = {
	{ "trunc", 1, MathShape::Componentwise, 3, kNoInstruction, kNoInstruction },
	{ "abs", 1, MathShape::Componentwise, 4, 5, kNoInstruction },
	{ "fabs", 1, MathShape::Componentwise, 4, kNoInstruction, kNoInstruction },
	{ "floor", 1, MathShape::Componentwise, 8, kNoInstruction, kNoInstruction },
	{ "ceil", 1, MathShape::Componentwise, 9, kNoInstruction, kNoInstruction },
	{ "fract", 1, MathShape::Componentwise, 10, kNoInstruction, kNoInstruction },
	{ "fmod", 2, MathShape::FloatRemainder, kNoInstruction, kNoInstruction, kNoInstruction },
	{ "round", 1, MathShape::RoundHalfAway, 3, kNoInstruction, kNoInstruction },
	{ "rint", 1, MathShape::Componentwise, 2, kNoInstruction, kNoInstruction },
	{ "sign", 1, MathShape::SignExact, 6, kNoInstruction, kNoInstruction, false },
	{ "sin", 1, MathShape::Componentwise, 13, kNoInstruction, kNoInstruction },
	{ "cos", 1, MathShape::Componentwise, 14, kNoInstruction, kNoInstruction },
	{ "tan", 1, MathShape::Componentwise, 15, kNoInstruction, kNoInstruction },
	{ "asin", 1, MathShape::Componentwise, 16, kNoInstruction, kNoInstruction },
	{ "acos", 1, MathShape::Componentwise, 17, kNoInstruction, kNoInstruction },
	{ "atan", 1, MathShape::Componentwise, 18, kNoInstruction, kNoInstruction },
	{ "atan2", 2, MathShape::Componentwise, 25, kNoInstruction, kNoInstruction },
	{ "pow", 2, MathShape::Componentwise, 26, kNoInstruction, kNoInstruction },
	// powr is pow for x >= 0 (MSL 6.6); GLSL.std.450 Pow is undefined for x < 0 too.
	{ "powr", 2, MathShape::Componentwise, 26, kNoInstruction, kNoInstruction },
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
	{ "fmin", 2, MathShape::Componentwise, 79, kNoInstruction, kNoInstruction },
	{ "fmax", 2, MathShape::Componentwise, 80, kNoInstruction, kNoInstruction },
	// NClamp (81) turns a NaN operand into the lower bound, as Metal does, where FClamp is
	// undefined for it; saturate below is a clamp to [0, 1].
	{ "clamp", 3, MathShape::Componentwise, 81, 45, 44 },
	{ "saturate", 1, MathShape::Saturate, 81, kNoInstruction, kNoInstruction },
	{ "mix", 3, MathShape::Componentwise, 46, kNoInstruction, kNoInstruction, false },
	{ "step", 2, MathShape::Componentwise, 48, kNoInstruction, kNoInstruction, false },
	{ "smoothstep", 3, MathShape::Componentwise, 49, kNoInstruction, kNoInstruction, false },
	{ "fma", 3, MathShape::Componentwise, 50, kNoInstruction, kNoInstruction },
	{ "length", 1, MathShape::VectorToScalar, 66, kNoInstruction, kNoInstruction },
	{ "distance", 2, MathShape::VectorToScalar, 67, kNoInstruction, kNoInstruction },
	{ "cross", 2, MathShape::Cross, 68, kNoInstruction, kNoInstruction, false },
	{ "normalize", 1, MathShape::Vector, 69, kNoInstruction, kNoInstruction },
	{ "reflect", 2, MathShape::Vector, 71, kNoInstruction, kNoInstruction, false },
	{ "refract", 3, MathShape::Refract, 72, kNoInstruction, kNoInstruction, false },
	{ "dot", 2, MathShape::Dot, kNoInstruction, kNoInstruction, kNoInstruction, false },
	{ "fwidth", 1, MathShape::Fwidth, kNoInstruction, kNoInstruction, kNoInstruction, false },
	{ "dfdx", 1, MathShape::Dfdx, kNoInstruction, kNoInstruction, kNoInstruction, false },
	{ "dfdy", 1, MathShape::Dfdy, kNoInstruction, kNoInstruction, kNoInstruction, false },
};

constexpr PackBuiltin kPackBuiltins[] = {
	{ "pack_float_to_unorm4x8", true, PackEncoding::Unorm, 4, 8, ScalarKind::Float },
	{ "pack_half_to_unorm4x8", true, PackEncoding::Unorm, 4, 8, ScalarKind::Half },
	{ "pack_float_to_snorm4x8", true, PackEncoding::Snorm, 4, 8, ScalarKind::Float },
	{ "pack_half_to_snorm4x8", true, PackEncoding::Snorm, 4, 8, ScalarKind::Half },
	{ "pack_float_to_srgb_unorm4x8", true, PackEncoding::Srgb, 4, 8, ScalarKind::Float },
	{ "pack_half_to_srgb_unorm4x8", true, PackEncoding::Srgb, 4, 8, ScalarKind::Half },
	{ "pack_float_to_unorm2x16", true, PackEncoding::Unorm, 2, 16, ScalarKind::Float },
	{ "pack_half_to_unorm2x16", true, PackEncoding::Unorm, 2, 16, ScalarKind::Half },
	{ "pack_float_to_snorm2x16", true, PackEncoding::Snorm, 2, 16, ScalarKind::Float },
	{ "pack_half_to_snorm2x16", true, PackEncoding::Snorm, 2, 16, ScalarKind::Half },
	{ "unpack_unorm4x8_to_float", false, PackEncoding::Unorm, 4, 8, ScalarKind::Float },
	{ "unpack_unorm4x8_to_half", false, PackEncoding::Unorm, 4, 8, ScalarKind::Half },
	{ "unpack_snorm4x8_to_float", false, PackEncoding::Snorm, 4, 8, ScalarKind::Float },
	{ "unpack_snorm4x8_to_half", false, PackEncoding::Snorm, 4, 8, ScalarKind::Half },
	{ "unpack_unorm4x8_srgb_to_float", false, PackEncoding::Srgb, 4, 8, ScalarKind::Float },
	{ "unpack_unorm4x8_srgb_to_half", false, PackEncoding::Srgb, 4, 8, ScalarKind::Half },
	{ "unpack_unorm2x16_to_float", false, PackEncoding::Unorm, 2, 16, ScalarKind::Float },
	{ "unpack_unorm2x16_to_half", false, PackEncoding::Unorm, 2, 16, ScalarKind::Half },
	{ "unpack_snorm2x16_to_float", false, PackEncoding::Snorm, 2, 16, ScalarKind::Float },
	{ "unpack_snorm2x16_to_half", false, PackEncoding::Snorm, 2, 16, ScalarKind::Half },
};

const PackBuiltin* findPackBuiltin(const std::string& name) {
	for (const PackBuiltin& builtin: kPackBuiltins) {
		if (name == builtin.name) {
			return &builtin;
		}
	}
	return nullptr;
}

// A construct whose type is void: the cast (void)e.
bool isVoidCast(const Expression& expression) {
	return expression.kind == ExpressionKind::Construct && expression.constructType
		&& expression.constructType->scalar == ScalarKind::Void
		&& !expression.constructType->isPointer && !expression.constructType->arrayLength
		&& expression.constructType->namedType.empty() && !expression.constructType->isVector()
		&& !expression.constructType->isMatrix() && expression.arguments.size() == 1;
}

// An expression of type void: a cast to void, a conditional whose arms are both
// void, or a comma whose last operand is.
bool isVoidValued(const Expression& expression) {
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

std::optional<MathBuiltin> findScopedMathBuiltin(const std::string& callee) {
	std::string_view rest = callee;
	if (rest.compare(0, 7, "metal::") == 0) {
		rest.remove_prefix(7);
		if (rest.compare(0, 9, "precise::") == 0 || rest.compare(0, 6, "fast::") == 0) {
			rest.remove_prefix(rest[0] == 'p' ? 9 : 6);
			const MathBuiltin* builtin = findMathBuiltin(std::string(rest));
			if (!builtin || !builtin->inFastPrecise) {
				return std::nullopt;
			}
			MathBuiltin scoped = *builtin;
			scoped.signedInstruction = kNoInstruction;
			scoped.unsignedInstruction = kNoInstruction;
			return scoped;
		}
	}
	if (const MathBuiltin* builtin = findMathBuiltin(std::string(rest))) {
		return *builtin;
	}
	return std::nullopt;
}

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
	if (name == "select" || name == "precise::select" || name == "fast::select") {
		if (operands.size() != 3 || !operands[0] || !operands[1] || !operands[2]) { return false; }
		const StdlibOperand a = *operands[0], b = *operands[1], c = *operands[2];
		const bool valueOk = name == "select"
			? (isFloatKind(a.scalar) || isIntegerKind(a.scalar) || a.scalar == ScalarKind::Bool)
			: (a.scalar == ScalarKind::Float || a.scalar == ScalarKind::Half);
		return valueOk && a == b && a.width <= 4 && c.scalar == ScalarKind::Bool
			&& (c.width == 1 || c.width == a.width);
	}
	const std::optional<MathBuiltin> builtin = findScopedMathBuiltin("metal::" + name);
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

template <typename Visit>
void visitExpressionIdentifiers(const Expression& expression, const Visit& visit) {
	if (expression.kind == ExpressionKind::Identifier) {
		visit(expression);
	}
	for (const Expression* child: { expression.left.get(), expression.right.get() }) {
		if (child && !(expression.kind == ExpressionKind::Call && child == expression.left.get()
			&& child->kind == ExpressionKind::Identifier)) {
			visitExpressionIdentifiers(*child, visit);
		}
	}
	for (const ExpressionPtr& argument: expression.arguments) {
		visitExpressionIdentifiers(*argument, visit);
	}
	for (const InitializerElement& element: expression.elements) {
		visitExpressionIdentifiers(*element.value, visit);
	}
}

// Every name an expression or statement refers to. A constexpr sampler nothing
// refers to has no state in Apple's AIR, so it gets no binding either.
// File-scope "::name" identifiers are excluded when includeFileScope is false so
// they do not mark a shadowing local or count against a local alias.
void collectIdentifiers(const Expression& expression, std::set<std::string>& names,
	bool includeFileScope) {
	visitExpressionIdentifiers(expression, [&](const Expression& id) {
		if (includeFileScope || !id.fileScope) {
			names.insert(id.name);
		}
	});
}

void collectIdentifiers(const Statement& statement, std::set<std::string>& names,
	bool includeFileScope) {
	for (const Expression* expression: { statement.expression.get(), statement.forCondition.get(),
		statement.forIncrement.get(), statement.whileCondition.get() }) {
		if (expression) collectIdentifiers(*expression, names, includeFileScope);
	}
	for (const std::optional<VariableDeclaration>* declaration: { &statement.declaration, &statement.forInitializer }) {
		if (*declaration && (*declaration)->initializer) {
			collectIdentifiers(*(*declaration)->initializer, names, includeFileScope);
		}
	}
	for (const StatementPtr& child: statement.children) {
		collectIdentifiers(*child, names, includeFileScope);
	}
	for (const Statement* nested: { statement.thenBranch.get(), statement.elseBranch.get(),
		statement.forBody.get(), statement.whileBody.get() }) {
		if (nested) collectIdentifiers(*nested, names, includeFileScope);
	}
	for (const SwitchCase& kase: statement.switchCases) {
		if (kase.value) collectIdentifiers(*kase.value, names, includeFileScope);
		for (const StatementPtr& child: kase.body) {
			collectIdentifiers(*child, names, includeFileScope);
		}
	}
	for (const StatementPtr& child: statement.switchPreamble) {
		collectIdentifiers(*child, names, includeFileScope);
	}
}

// The variables an expression or statement stores to or binds a reference to: the head of an assignment's
// target, of an increment's or decrement's operand, and of a reference's initialiser.
const Expression& writtenRoot(const Expression& target) {
	const Expression* root = &target;
	while ((root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) && root->left) {
		root = root->left.get();
	}
	return *root;
}

// A function name without its qualification.
std::string unqualifiedName(const std::string& name) {
	const size_t at = name.rfind("::");
	return at == std::string::npos ? name : name.substr(at + 2);
}

// The argument positions, by function name, that some function of that name takes by a mutable
// reference. Overloads are not resolved here, so any of them counts.
using ReferenceSlots = std::set<std::pair<std::string, size_t>>;

void collectWritten(const Expression& expression, const ReferenceSlots& references, std::set<std::string>& names) {
	const bool step = expression.kind == ExpressionKind::Unary
		&& (expression.unaryOperator == UnaryOperator::PreIncrement || expression.unaryOperator == UnaryOperator::PreDecrement
			|| expression.unaryOperator == UnaryOperator::PostIncrement
			|| expression.unaryOperator == UnaryOperator::PostDecrement);
	if ((expression.kind == ExpressionKind::Assign || step) && expression.left) {
		const Expression& root = writtenRoot(*expression.left);
		if (root.kind == ExpressionKind::Identifier) {
			names.insert(root.name);
		}
	}
	// An argument a callee may bind a mutable reference to.
	if (expression.kind == ExpressionKind::Call && expression.left) {
		const Expression& callee = *expression.left;
		const std::string name = callee.kind == ExpressionKind::Identifier ? unqualifiedName(callee.name)
			: callee.kind == ExpressionKind::Member ? callee.name : std::string();
		for (size_t i = 0; !name.empty() && i < expression.arguments.size(); ++i) {
			const Expression& root = writtenRoot(*expression.arguments[i]);
			if (root.kind == ExpressionKind::Identifier && references.count({ name, i })) {
				names.insert(root.name);
			}
		}
	}
	for (const Expression* child: { expression.left.get(), expression.right.get() }) {
		if (child) collectWritten(*child, references, names);
	}
	for (const ExpressionPtr& argument: expression.arguments) {
		collectWritten(*argument, references, names);
	}
	for (const InitializerElement& element: expression.elements) {
		collectWritten(*element.value, references, names);
	}
}

void collectWritten(const Statement& statement, const ReferenceSlots& references, std::set<std::string>& names) {
	for (const Expression* expression: { statement.expression.get(), statement.forCondition.get(),
		statement.forIncrement.get(), statement.whileCondition.get() }) {
		if (expression) collectWritten(*expression, references, names);
	}
	for (const std::optional<VariableDeclaration>* declaration: { &statement.declaration, &statement.forInitializer }) {
		if (*declaration && (*declaration)->initializer) {
			collectWritten(*(*declaration)->initializer, references, names);
			if ((*declaration)->isReference) {
				const Expression& root = writtenRoot(*(*declaration)->initializer);
				if (root.kind == ExpressionKind::Identifier) {
					names.insert(root.name);
				}
			}
		}
	}
	for (const StatementPtr& child: statement.children) {
		collectWritten(*child, references, names);
	}
	for (const Statement* nested: { statement.thenBranch.get(), statement.elseBranch.get(),
		statement.forBody.get(), statement.whileBody.get() }) {
		if (nested) collectWritten(*nested, references, names);
	}
	for (const SwitchCase& kase: statement.switchCases) {
		for (const StatementPtr& child: kase.body) {
			collectWritten(*child, references, names);
		}
	}
	for (const StatementPtr& child: statement.switchPreamble) {
		collectWritten(*child, references, names);
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
// the file-scope name from its declaration on, its own initialiser included,
// unless written "::name" (fileScope).
using ScopeStack = std::vector<std::set<std::string>>;

void collectUnshadowed(const Expression* expression, const ScopeStack& scopes, std::set<std::string>& names) {
	if (!expression) return;
	visitExpressionIdentifiers(*expression, [&](const Expression& id) {
		const bool hidden = !id.fileScope && std::any_of(scopes.begin(), scopes.end(),
			[&](const std::set<std::string>& scope) { return scope.count(id.name) != 0; });
		if (!hidden) names.insert(id.name);
	});
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

} // namespace mslc
