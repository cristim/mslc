#include "preprocessor/internal.h"

namespace mslc {

namespace pp {

int precedence(BinaryOp op) {
	switch (op) {
		case BinaryOp::Mul: case BinaryOp::Div: case BinaryOp::Mod: return 10;
		case BinaryOp::Add: case BinaryOp::Sub: return 9;
		case BinaryOp::Shl: case BinaryOp::Shr: return 8;
		case BinaryOp::Lt: case BinaryOp::Le: case BinaryOp::Gt: case BinaryOp::Ge: return 7;
		case BinaryOp::Eq: case BinaryOp::Ne: return 6;
		case BinaryOp::BitAnd: return 5;
		case BinaryOp::BitXor: return 4;
		case BinaryOp::BitOr: return 3;
		case BinaryOp::LogicalAnd: return 2;
		case BinaryOp::LogicalOr: return 1;
		case BinaryOp::None: break;
	}
	return 0;
}

BinaryOp binaryOpFor(const Token& token) {
	switch (token.kind) {
		case TokenKind::Star: return BinaryOp::Mul;
		case TokenKind::Slash: return BinaryOp::Div;
		case TokenKind::Percent: return BinaryOp::Mod;
		case TokenKind::Plus: return BinaryOp::Add;
		case TokenKind::Minus: return BinaryOp::Sub;
		case TokenKind::ShiftLeft: return BinaryOp::Shl;
		case TokenKind::ShiftRight: return BinaryOp::Shr;
		case TokenKind::Less: return BinaryOp::Lt;
		case TokenKind::LessEqual: return BinaryOp::Le;
		case TokenKind::Greater: return BinaryOp::Gt;
		case TokenKind::GreaterEqual: return BinaryOp::Ge;
		case TokenKind::Equal: return BinaryOp::Eq;
		case TokenKind::NotEqual: return BinaryOp::Ne;
		case TokenKind::Ampersand: return BinaryOp::BitAnd;
		case TokenKind::Caret: return BinaryOp::BitXor;
		case TokenKind::Pipe: return BinaryOp::BitOr;
		case TokenKind::AndAnd: return BinaryOp::LogicalAnd;
		case TokenKind::OrOr: return BinaryOp::LogicalOr;
		case TokenKind::Identifier:
			if (token.text == "and") return BinaryOp::LogicalAnd;
			if (token.text == "or") return BinaryOp::LogicalOr;
			if (token.text == "bitand") return BinaryOp::BitAnd;
			if (token.text == "xor") return BinaryOp::BitXor;
			if (token.text == "bitor") return BinaryOp::BitOr;
			if (token.text == "not_eq") return BinaryOp::Ne;
			return BinaryOp::None;
		default: return BinaryOp::None;
	}
}

} // namespace pp

using namespace pp;

bool Preprocessor::evaluateCondition(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	const size_t count = line.size();
	if (count < 2) {
		fail(at, "#if with no expression");
	}

	std::deque<PPTok> work;
	for (size_t i = 1; i < count;) {
		const Token& token = line[i];

		if (isName(token) && token.text == "defined") {
			size_t j = i + 1;
			bool parenthesised = false;
			if (j < count && line[j].kind == TokenKind::LParen) {
				parenthesised = true;
				++j;
			}
			if (j >= count || !isName(line[j])) {
				fail(locate(cursor, token), "operator 'defined' requires an identifier");
			}
			const bool defined = isDefinedName(line[j].text, locate(cursor, line[j]));
			++j;
			if (parenthesised) {
				if (j >= count || line[j].kind != TokenKind::RParen) {
					fail(locate(cursor, token), "missing ')' after 'defined'");
				}
				++j;
			}

			work.push_back(synthesize(defined ? "1" : "0", fromFile(cursor, token)));
			i = j;
			continue;
		}

		if (isName(token) && token.text == "__has_include") {
			if (i + 1 >= count || line[i + 1].kind != TokenKind::LParen) {
				fail(locate(cursor, token), "missing '(' after '__has_include'");
			}

			size_t close = i + 2;
			while (close < count && line[close].kind != TokenKind::RParen) {
				++close;
			}
			if (close >= count) {
				fail(locate(cursor, token), "missing ')' after '__has_include'");
			}

			const std::vector<Token> argument(line.begin() + i + 2, line.begin() + close);
			const Loc here = locate(cursor, token);
			const Header header = parseHeaderName(argument, 0, here, "#include");

			bool found = false;
			if (header.angled) {
				if (!builtinHeaderText(header.name)) {
					fail(here, "__has_include(" + header.name + ") cannot be answered: mslc provides "
						+ kBuiltinHeaderList + " only");
				}
				found = true;
			} else {
				found = resolveQuoted(cursor.file, header.name, here, "#include") != nullptr;
			}

			work.push_back(synthesize(found ? "1" : "0", fromFile(cursor, token)));
			i = close + 1;
			continue;
		}

		work.push_back(fromFile(cursor, token));
		++i;
	}

	Expression expr;
	scan(nullptr, work, expr.tokens, 1);
	expr.at = at;

	if (expr.tokens.empty()) {
		fail(at, "#if with no expression");
	}

	const Value value = parseTernary(expr, true);
	if (expr.position < expr.tokens.size()) {
		const PPTok& extra = expr.tokens[expr.position];
		if (extra.tok.kind == TokenKind::RParen) {
			fail(extra.loc, "unmatched ')' in the #if expression");
		}
		fail(extra.loc, "missing a binary operator before \"" + std::string(extra.tok.text)
			+ "\" in the #if expression");
	}
	return value.truthy();
}

Value Preprocessor::applyBinary(BinaryOp op, Value a, Value b, bool live, const Loc& at) const {
	Value result;
	const bool unsignedResult = a.isUnsigned || b.isUnsigned;
	result.isUnsigned = unsignedResult;

	auto boolean = [&](bool value) {
		Value v;
		v.bits = value ? 1 : 0;
		return v;
	};

	switch (op) {
		case BinaryOp::Mul:
			result.bits = a.bits * b.bits;
			return result;
		case BinaryOp::Add:
			result.bits = a.bits + b.bits;
			return result;
		case BinaryOp::Sub:
			result.bits = a.bits - b.bits;
			return result;

		case BinaryOp::Div:
		case BinaryOp::Mod: {
			if (b.bits == 0) {
				if (live) {
					fail(at, op == BinaryOp::Div ? "division by zero in the #if expression"
						: "remainder by zero in the #if expression");
				}
				return result;
			}
			if (unsignedResult) {
				result.bits = op == BinaryOp::Div ? a.bits / b.bits : a.bits % b.bits;
			} else if (a.bits == kSignBit && b.asSigned() == -1) {
				result.bits = op == BinaryOp::Div ? a.bits : 0;
			} else {
				result.bits = static_cast<Wide>(op == BinaryOp::Div ? a.asSigned() / b.asSigned()
					: a.asSigned() % b.asSigned());
			}
			return result;
		}

		case BinaryOp::Shl:
		case BinaryOp::Shr: {
			// The result has the left operand's type. The count is read as unsigned, so
			// a negative one is past the width like any other that is too large.
			result.isUnsigned = a.isUnsigned;
			const Wide count = b.bits;

			// Apple's compiler truncates a right-shift count to 32 bits first; mslc does not.
			if (op == BinaryOp::Shl) {
				result.bits = count >= kWideBits ? 0 : a.bits << static_cast<unsigned>(count);
			} else {
				// A right shift by the width or more leaves the sign bit, as it does in
				// Apple's compiler, rather than shifting everything out.
				const unsigned shift = count >= kWideBits ? kWideBits - 1 : static_cast<unsigned>(count);
				result.bits = a.isUnsigned ? a.bits >> shift : static_cast<Wide>(a.asSigned() >> shift);
			}
			return result;
		}

		case BinaryOp::BitAnd: result.bits = a.bits & b.bits; return result;
		case BinaryOp::BitXor: result.bits = a.bits ^ b.bits; return result;
		case BinaryOp::BitOr: result.bits = a.bits | b.bits; return result;

		case BinaryOp::Lt:
			return boolean(unsignedResult ? a.bits < b.bits : a.asSigned() < b.asSigned());
		case BinaryOp::Le:
			return boolean(unsignedResult ? a.bits <= b.bits : a.asSigned() <= b.asSigned());
		case BinaryOp::Gt:
			return boolean(unsignedResult ? a.bits > b.bits : a.asSigned() > b.asSigned());
		case BinaryOp::Ge:
			return boolean(unsignedResult ? a.bits >= b.bits : a.asSigned() >= b.asSigned());
		case BinaryOp::Eq: return boolean(a.bits == b.bits);
		case BinaryOp::Ne: return boolean(a.bits != b.bits);
		case BinaryOp::LogicalAnd: return boolean(a.truthy() && b.truthy());
		case BinaryOp::LogicalOr: return boolean(a.truthy() || b.truthy());
		case BinaryOp::None: break;
	}
	return result;
}

Value Preprocessor::parseTernary(Expression& expr, bool live) {
	if (++expr.depth > kMaxExpressionDepth) {
		fail(expr.at, "the #if expression is nested too deeply");
	}

	const Value condition = parseBinary(expr, 1, live);
	Value result = condition;

	if (expr.position < expr.tokens.size() && expr.tokens[expr.position].tok.kind == TokenKind::Question) {
		++expr.position;
		const Value whenTrue = parseTernary(expr, live && condition.truthy());

		if (expr.position >= expr.tokens.size() || expr.tokens[expr.position].tok.kind != TokenKind::Colon) {
			fail(expr.position < expr.tokens.size() ? expr.tokens[expr.position].loc : expr.at,
				"expected ':' in the #if conditional expression");
		}
		++expr.position;
		const Value whenFalse = parseTernary(expr, live && !condition.truthy());

		result = condition.truthy() ? whenTrue : whenFalse;
		result.isUnsigned = whenTrue.isUnsigned || whenFalse.isUnsigned;
	}

	--expr.depth;
	return result;
}

Value Preprocessor::parseBinary(Expression& expr, int minPrecedence, bool live) {
	Value lhs = parseUnary(expr, live);

	while (expr.position < expr.tokens.size()) {
		const PPTok& opToken = expr.tokens[expr.position];
		const BinaryOp op = binaryOpFor(opToken.tok);
		if (op == BinaryOp::None || precedence(op) < minPrecedence) {
			break;
		}
		++expr.position;

		// The right side of && and || is parsed but not evaluated when the left
		// decides the result, so a division by zero there is not an error.
		bool rhsLive = live;
		if (op == BinaryOp::LogicalAnd) {
			rhsLive = live && lhs.truthy();
		} else if (op == BinaryOp::LogicalOr) {
			rhsLive = live && !lhs.truthy();
		}

		const Value rhs = parseBinary(expr, precedence(op) + 1, rhsLive);
		lhs = applyBinary(op, lhs, rhs, live, opToken.loc);
	}

	return lhs;
}

Value Preprocessor::parseIntegerLiteral(Expression& expr) {
	const PPTok& first = expr.tokens[expr.position];
	std::string text(first.tok.text);
	++expr.position;

	// "1ull" and "0b101" arrive as a number followed by a name with no gap.
	while (expr.position < expr.tokens.size()) {
		const Token& next = expr.tokens[expr.position].tok;
		if (next.spaceBefore || !(isName(next) || next.kind == TokenKind::IntegerLiteral)) {
			break;
		}
		text += next.text;
		++expr.position;
	}

	size_t i = 0;
	unsigned base = 10;
	if (text.size() > 1 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
		base = 16;
		i = 2;
	} else if (text.size() > 1 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
		base = 2;
		i = 2;
	} else if (text.size() > 1 && text[0] == '0') {
		base = 8;
		i = 1;
	}

	Wide value = 0;
	bool any = base == 8;
	for (; i < text.size(); ++i) {
		const char c = text[i];
		unsigned digit = 0;
		if (c >= '0' && c <= '9') {
			digit = static_cast<unsigned>(c - '0');
		} else if (base == 16 && std::isxdigit(static_cast<unsigned char>(c))) {
			digit = static_cast<unsigned>(std::tolower(c) - 'a') + 10;
		} else {
			break;
		}

		if (digit >= base) {
			fail(first.loc, std::string("invalid digit '") + c + "' in " + (base == 8 ? "octal" : "binary")
				+ " constant");
		}
		if (value > (kWideMax - digit) / base) {
			fail(first.loc, "integer literal is too large to be represented in any integer type");
		}
		value = value * base + digit;
		any = true;
	}

	if (!any) {
		fail(first.loc, "invalid integer constant \"" + text + "\"");
	}

	std::string suffix = text.substr(i);
	std::transform(suffix.begin(), suffix.end(), suffix.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	static const char* const allowed[] = { "", "u", "l", "ul", "lu", "ll", "ull", "llu" };
	if (std::none_of(std::begin(allowed), std::end(allowed), [&](const char* s) { return suffix == s; })) {
		fail(first.loc, "invalid suffix \"" + text.substr(i) + "\" on integer constant");
	}

	Value result;
	result.bits = value;
	result.isUnsigned = suffix.find('u') != std::string::npos || value >= kSignBit;
	return result;
}

Value Preprocessor::parseUnary(Expression& expr, bool live) {
	if (expr.position >= expr.tokens.size()) {
		fail(expr.at, "expected a value in the #if expression");
	}

	if (++expr.depth > kMaxExpressionDepth) {
		fail(expr.at, "the #if expression is nested too deeply");
	}

	const PPTok& current = expr.tokens[expr.position];
	const Token& token = current.tok;
	Value result;

	auto unary = [&](auto apply) {
		++expr.position;
		Value operand = parseUnary(expr, live);
		result = apply(operand);
	};

	if (token.kind == TokenKind::Plus) {
		unary([](Value v) { return v; });
	} else if (token.kind == TokenKind::Minus) {
		unary([](Value v) { v.bits = 0 - v.bits; return v; });
	} else if (token.kind == TokenKind::Tilde || (isName(token) && token.text == "compl")) {
		unary([](Value v) { v.bits = ~v.bits; return v; });
	} else if (token.kind == TokenKind::Bang || (isName(token) && token.text == "not")) {
		unary([](Value v) { Value r; r.bits = v.truthy() ? 0 : 1; return r; });
	} else if (token.kind == TokenKind::LParen) {
		++expr.position;
		result = parseTernary(expr, live);

		// A comma inside parentheses evaluates both sides and yields the right.
		while (expr.position < expr.tokens.size() && expr.tokens[expr.position].tok.kind == TokenKind::Comma) {
			++expr.position;
			result = parseTernary(expr, live);
		}

		if (expr.position >= expr.tokens.size() || expr.tokens[expr.position].tok.kind != TokenKind::RParen) {
			fail(current.loc, "missing ')' in the #if expression");
		}
		++expr.position;
	} else if (token.kind == TokenKind::IntegerLiteral
		|| (token.kind == TokenKind::Invalid && token.invalidReason == InvalidReason::MalformedNumber)) {
		result = parseIntegerLiteral(expr);
	} else if (token.kind == TokenKind::FloatLiteral) {
		fail(current.loc, "floating constant in the #if expression");
	} else if (isName(token)) {
		if (token.text == "defined") {
			fail(current.loc, "'defined' produced by macro expansion is not supported");
		}

		// An identifier that is still here after expansion is not a macro, which
		// C reads as 0. true and false are C++ keywords with their own values.
		result.bits = token.text == "true" ? 1 : 0;
		++expr.position;
	} else if (token.kind == TokenKind::Invalid && token.text == "'") {
		fail(current.loc, "character constants in the #if expression are not supported");
	} else {
		fail(current.loc, "token \"" + std::string(token.text) + "\" is not valid in the #if expression");
	}

	--expr.depth;
	return result;
}

} // namespace mslc
