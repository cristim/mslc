#include "sema/emitter.h"

namespace mslc {

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
		case ExpressionKind::Call:
		case ExpressionKind::Construct: {
			const Type* asTypeTarget = expression.kind == ExpressionKind::Call
				&& isAsTypeCall(expression) && expression.left->templateArguments.size() == 1
				&& expression.left->templateArguments.front().isType && expression.arguments.size() == 1
				? &expression.left->templateArguments.front().type : nullptr;
			if (asTypeTarget) {
				// as_type<T>(x) keeps the operand's bits: read them back as T, as Apple folds it.
				if (!asTypeTarget->isScalar() || asTypeTarget->isMatrix() || asTypeTarget->isPointer
					|| asTypeTarget->arrayLength || !asTypeTarget->namedType.empty() || !asTypeTarget->enumName.empty()) {
					throw CompileError("a constant's initialiser has to be a value that can be worked out "
						"from the declarations before it, and this is not one");
				}
				requireEnumValue("", *expression.arguments.front(), "as_type<" + typeName(*asTypeTarget) + ">");
				const FoldedConstant operand = foldExpression(*expression.arguments.front());
				const bool numeric = !operand.isComposite && operand.scalar != ScalarKind::Bool
					&& operand.scalar != ScalarKind::Double;
				const ScalarKind target = asTypeTarget->scalar;
				if (!numeric || target == ScalarKind::Double || target == ScalarKind::Bool) {
					throw CompileError("a constant's initialiser has to be a value that can be worked out "
						"from the declarations before it, and this is not one");
				}
				const uint32_t width = mappingFor(operand.scalar).width;
				if (width != mappingFor(target).width) {
					throw CompileError("as_type<" + typeName(*asTypeTarget) + "> needs an operand of the same size");
				}

				// A NaN is folded as a double, which keeps neither its sign nor its payload, so its bits
				// are not known here; refuse rather than fold a different pattern.
				if (isFloatKind(operand.scalar) && std::isnan(operand.number)) {
					throw CompileError("as_type of a NaN in a constant is not lowered yet, since its payload is not kept");
				}
				uint64_t bits = 0;
				if (operand.scalar == ScalarKind::Half) {
					bits = halfBitsOf(operand.number);
				} else if (operand.scalar == ScalarKind::Float) {
					const auto narrowed = static_cast<float>(operand.number);
					uint32_t word = 0;
					std::memcpy(&word, &narrowed, sizeof(word));
					bits = word;
				} else {
					bits = width == 64 ? operand.integer : operand.integer & ((uint64_t{ 1 } << width) - 1);
				}

				FoldedConstant folded;
				folded.scalar = target;
				if (target == ScalarKind::Half) {
					folded.number = halfValueOf(static_cast<uint32_t>(bits));
				} else if (target == ScalarKind::Float) {
					const auto word = static_cast<uint32_t>(bits);
					float value = 0.0f;
					std::memcpy(&value, &word, sizeof(value));
					folded.number = value;
				} else {
					folded.integer = normalizeInteger(target, bits);
				}
				if (isFloatKind(target) && std::isnan(folded.number)) {
					throw CompileError("as_type to a NaN in a constant is not lowered yet, since its payload is not kept");
				}
				return folded;
			}
			if (expression.kind == ExpressionKind::Call) {
				break;
			}
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
		}
		default: break;
	}

	throw CompileError("a constant's initialiser has to be a value that can be worked out "
		"from the declarations before it, and this is not one");
}

// c ? a : b of constants. Only the chosen arm is evaluated, but the result has the
// type both arms meet in, so an arm that does not fold is emitted and lifted out again
// for its type, which needs a function to emit into.
FoldedConstant Emitter::foldConditional(const Expression& expression) {
	checkConditionalOperands(*expression.left, *expression.arguments[0], *expression.arguments[1]);
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

// A folded bool as the int it promotes to: 1 for true, 0 for false.
static FoldedConstant promotedBool(const FoldedConstant& value) {
	FoldedConstant promoted = value;
	promoted.scalar = ScalarKind::Int;
	promoted.integer = value.boolean ? 1 : 0;
	return promoted;
}

FoldedConstant Emitter::foldUnary(const Expression& expression) {
	if (isComponentValue(*expression.left)) {
		throw CompileError("unary operator does not accept a value of scoped enum metal::component without a cast");
	}
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

	// A bool is promoted to int ([conv.prom]), as emitUnary promotes it.
	if (isBool) {
		operand = promotedBool(operand);
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
	checkBinaryComponentOperands(expression.binaryOperator, *expression.left, *expression.right);
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

	FoldedConstant left = foldExpression(*expression.left);
	FoldedConstant right = foldExpression(*expression.right);

	if (left.isComposite || right.isComposite) {
		throw CompileError("a composite constant cannot be an operand of a binary operator");
	}

	const BinaryOperator op = expression.binaryOperator;

	if (isComparisonOperator(op)) {
		return foldComparison(op, left, right);
	}

	// A bool operand is promoted to int ([conv.prom]), as emitBinary promotes it.
	if (left.scalar == ScalarKind::Bool) {
		left = promotedBool(left);
	}
	if (right.scalar == ScalarKind::Bool) {
		right = promotedBool(right);
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

} // namespace mslc
