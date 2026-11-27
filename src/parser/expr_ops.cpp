#include "parser/internal.h"

namespace mslc {

ExpressionPtr Parser::parseExpression() {
	auto left = parseAssignment();
	while (at(TokenKind::Comma)) {
		advance();
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Comma;
		expression->line = left->line;
		expression->left = std::move(left);
		expression->right = parseAssignment();
		measure(*expression);
		left = std::move(expression);
	}
	return left;
}

ExpressionPtr Parser::parseAssignment() {
	const NestingScope scope(*this);
	auto left = parseConditional();

	static const std::pair<TokenKind, BinaryOperator> compounds[] = {
		{ TokenKind::PlusAssign, BinaryOperator::Add },
		{ TokenKind::MinusAssign, BinaryOperator::Subtract },
		{ TokenKind::StarAssign, BinaryOperator::Multiply },
		{ TokenKind::SlashAssign, BinaryOperator::Divide },
		{ TokenKind::PercentAssign, BinaryOperator::Modulo },
		{ TokenKind::AmpersandAssign, BinaryOperator::BitAnd },
		{ TokenKind::PipeAssign, BinaryOperator::BitOr },
		{ TokenKind::CaretAssign, BinaryOperator::BitXor },
		{ TokenKind::ShiftLeftAssign, BinaryOperator::ShiftLeft },
		{ TokenKind::ShiftRightAssign, BinaryOperator::ShiftRight },
	};

	std::optional<BinaryOperator> compound;
	for (const auto& [token, op]: compounds) {
		if (at(token)) {
			compound = op;
		}
	}

	if (at(TokenKind::Assign) || compound) {
		advance();
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Assign;
		expression->compoundOperator = compound;
		expression->line = left->line;
		expression->left = std::move(left);
		// "a = {1, 2, 3};" assigns a braced list, which the emitter takes for an array<T, N> only.
		expression->right = !compound && at(TokenKind::LBrace) ? parseInitializerList() : parseAssignment();
		measure(*expression);
		return expression;
	}

	return left;
}

// `c ? a : b` binds looser than ||, and right to left: the false arm is itself
// an assignment-expression, so `c ? a : d ? b : e` nests to the right and
// `c ? a : x = b` assigns inside the arm, as C++ parses it.
ExpressionPtr Parser::parseConditional() {
	auto condition = parseLogicalOr();
	if (!at(TokenKind::Question)) {
		return condition;
	}

	advance();
	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::Conditional;
	expression->line = condition->line;
	expression->left = std::move(condition);
	expression->arguments.push_back(parseExpression());
	expect(TokenKind::Colon, "between the two values of a conditional");
	expression->arguments.push_back(parseAssignment());
	measure(*expression);
	return expression;
}

ExpressionPtr Parser::parseLogicalOr() {
	return parseBinaryLevel([](Parser& p) { return p.parseLogicalAnd(); }, { { TokenKind::OrOr, BinaryOperator::LogicalOr } });
}

ExpressionPtr Parser::parseLogicalAnd() {
	return parseBinaryLevel([](Parser& p) { return p.parseBitwiseOr(); }, { { TokenKind::AndAnd, BinaryOperator::LogicalAnd } });
}

ExpressionPtr Parser::parseBitwiseOr() {
	return parseBinaryLevel([](Parser& p) { return p.parseBitwiseXor(); }, { { TokenKind::Pipe, BinaryOperator::BitOr } });
}

ExpressionPtr Parser::parseBitwiseXor() {
	return parseBinaryLevel([](Parser& p) { return p.parseBitwiseAnd(); }, { { TokenKind::Caret, BinaryOperator::BitXor } });
}

ExpressionPtr Parser::parseBitwiseAnd() {
	return parseBinaryLevel([](Parser& p) { return p.parseEquality(); }, { { TokenKind::Ampersand, BinaryOperator::BitAnd } });
}

ExpressionPtr Parser::parseEquality() {
	return parseBinaryLevel([](Parser& p) { return p.parseRelational(); }, { { TokenKind::Equal, BinaryOperator::Equal }, { TokenKind::NotEqual, BinaryOperator::NotEqual } });
}

// Inside the angle brackets of a trait's argument a ">" closes the list, so it is no operator there
// (C++ reads it the same way); a parenthesised operand is a fresh expression.
ExpressionPtr Parser::parseRelational() {
	if (_noGreater) {
		return parseBinaryLevel([](Parser& p) { return p.parseShift(); }, { { TokenKind::Less, BinaryOperator::Less },
			{ TokenKind::LessEqual, BinaryOperator::LessEqual }, { TokenKind::GreaterEqual, BinaryOperator::GreaterEqual } });
	}
	return parseBinaryLevel([](Parser& p) { return p.parseShift(); }, { { TokenKind::Less, BinaryOperator::Less }, { TokenKind::LessEqual, BinaryOperator::LessEqual },
		  { TokenKind::Greater, BinaryOperator::Greater }, { TokenKind::GreaterEqual, BinaryOperator::GreaterEqual } });
}

ExpressionPtr Parser::parseShift() {
	if (_noGreater) {
		return parseBinaryLevel([](Parser& p) { return p.parseAdditive(); }, { { TokenKind::ShiftLeft, BinaryOperator::ShiftLeft } });
	}
	return parseBinaryLevel([](Parser& p) { return p.parseAdditive(); }, { { TokenKind::ShiftLeft, BinaryOperator::ShiftLeft }, { TokenKind::ShiftRight, BinaryOperator::ShiftRight } });
}

ExpressionPtr Parser::parseAdditive() {
	return parseBinaryLevel([](Parser& p) { return p.parseMultiplicative(); }, { { TokenKind::Plus, BinaryOperator::Add }, { TokenKind::Minus, BinaryOperator::Subtract } });
}

ExpressionPtr Parser::parseMultiplicative() {
	return parseBinaryLevel([](Parser& p) { return p.parseUnary(); }, { { TokenKind::Star, BinaryOperator::Multiply }, { TokenKind::Slash, BinaryOperator::Divide },
		  { TokenKind::Percent, BinaryOperator::Modulo } });
}

// sizeof(type) is the type's size under Metal's layout, folded here to a ulong literal so every
// later stage sees a constant. A bare type name and a parenthesised expression both start with
// "(", so a type is taken only where one can begin and ")" follows it.
ExpressionPtr Parser::parseSizeof() {
	const size_t startLine = line();
	advance();

	if (at(TokenKind::LParen)) {
		const size_t open = _position;
		advance();

		size_t nameTokens = 0;
		Type probe;
		const std::string name = kind() == TokenKind::Identifier || at(TokenKind::ColonColon)
			? peekResolved(nameTokens) : std::string();
		const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
			&& isLocal(name);
		const bool inOwnBody = _structBody && name == _structBody->name;
		const bool startsType = inOwnBody || (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text))
			|| ((kind() == TokenKind::Identifier || at(TokenKind::ColonColon)) && !objectHidesTag(name)
				&& !(kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon && memberHidesName(name))
				&& (resolveTypeName(name, probe) || isTypeQualifier(name) || isResourceTypeName(name)
					|| (_unit.findStruct(name) != nullptr && !localHidesStruct) || (!localHidesStruct && (atStdArray(nameTokens) || atLibraryVec(nameTokens) || atTraitType(nameTokens)))))
			|| atKeyword("device") || atKeyword("constant") || atKeyword("threadgroup") || atKeyword("thread");
		if (startsType) {
			Type type = parseType(true, false);
			// parseType reads the first array length only; float[2][3] has more.
			while (type.arrayLength && at(TokenKind::LBracket)) {
				advance();
				const int64_t length = parseConstantIndex("an array length");
				if (length <= 0) {
					throw CompileError("zero-length arrays are not permitted in C++");
				}
				type.innerArrayLengths.push_back(static_cast<uint32_t>(length));
				expect(TokenKind::RBracket, "to close an array length");
			}
			if (at(TokenKind::RParen)) {
				advance();
				if (_structBody && _structBody->name == type.namedType && !type.isPointer) {
					throw CompileError("sizeof of the struct \"" + type.namedType + "\" inside its own body, "
						"where it is incomplete");
				}
				auto expression = std::make_unique<Expression>();
				expression->kind = ExpressionKind::IntLiteral;
				expression->line = startLine;
				expression->intKind = ScalarKind::ULong;
				expression->isSizeof = true;
				if (isDeductionMarker(type) && !type.isPointer && _inEnableIf) {
					// An enable_if condition read before T is deduced: the read with T decides.
					_dependentCondition = true;
					expression->intValue = 1;
					return expression;
				}
				expression->intValue = sizeofType(type, _unit);
				return expression;
			}
		}
		_position = open;
	}

	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::Sizeof;
	expression->line = startLine;
	const NestingScope scope(*this);
	expression->left = parseUnary();
	// A declared name is sized here, where the scope that decides which declaration it finds is open.
	if (const std::optional<Type> declared = declaredObjectType(*expression->left)) {
		expression->kind = ExpressionKind::IntLiteral;
		expression->intKind = ScalarKind::ULong;
		expression->isSizeof = true;
		expression->intValue = sizeofType(*declared, _unit);
		expression->left.reset();
		return expression;
	}
	measure(*expression);
	return expression;
}

ExpressionPtr Parser::parseUnary() {
	if (atKeyword("sizeof")) {
		return parseSizeof();
	}

	// "*this" is the object itself, which a member function holds by value.
	if (at(TokenKind::Star) && lookahead().kind == TokenKind::Identifier && lookahead().text == "this"
		&& isLocal("this")) {
		advance();
		return parseUnary();
	}

	// Only in prefix position: a '*' between two operands never gets here, because
	// parseMultiplicative consumes it after the left operand is complete.
	if (at(TokenKind::Star)) {
		auto dereference = std::make_unique<Expression>();
		dereference->kind = ExpressionKind::Index;
		dereference->isDereference = true;
		dereference->line = line();
		advance();

		const NestingScope scope(*this);
		auto operand = parseUnary();
		if (operand->kind == ExpressionKind::Binary) {
			throw CompileError("pointer arithmetic such as \"*(p + i)\" is not supported; "
				"index the pointer instead, \"p[i]\"");
		}
		if (operand->isDereference) {
			throw CompileError("dereferencing a pointer to a pointer is not supported");
		}

		auto zero = std::make_unique<Expression>();
		zero->kind = ExpressionKind::IntLiteral;
		zero->line = dereference->line;

		dereference->left = std::move(operand);
		dereference->arguments.push_back(std::move(zero));
		measure(*dereference);
		return dereference;
	}

	if (at(TokenKind::Ampersand)) {
		// The address of an element is not a value mslc holds; the atomic_*_explicit
		// functions take one, and the emitter refuses it anywhere else.
		auto address = std::make_unique<Expression>();
		address->kind = ExpressionKind::Unary;
		address->unaryOperator = UnaryOperator::AddressOf;
		address->line = line();
		advance();
		const NestingScope scope(*this);
		address->left = parseUnary();
		measure(*address);
		return address;
	}

	if (at(TokenKind::Minus) || at(TokenKind::Plus) || at(TokenKind::Bang) || at(TokenKind::Tilde)
		|| at(TokenKind::Increment) || at(TokenKind::Decrement)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Unary;
		expression->line = line();

		switch (kind()) {
			case TokenKind::Minus: expression->unaryOperator = UnaryOperator::Negate; break;
			case TokenKind::Plus: expression->unaryOperator = UnaryOperator::Plus; break;
			case TokenKind::Bang: expression->unaryOperator = UnaryOperator::Not; break;
			case TokenKind::Tilde: expression->unaryOperator = UnaryOperator::BitNot; break;
			case TokenKind::Increment: expression->unaryOperator = UnaryOperator::PreIncrement; break;
			default: expression->unaryOperator = UnaryOperator::PreDecrement; break;
		}

		advance();
		const NestingScope scope(*this);
		expression->left = parseUnary();
		measure(*expression);
		return expression;
	}

	if (at(TokenKind::LParen)) {
		if (auto cast = parseCast()) {
			return cast;
		}
	}

	return parsePostfix();
}

// A nonconsuming null result lets a speculative C-style cast backtrack.
std::optional<Type> Parser::peekCastTargetName(size_t& nameTokens) {
	const std::string head(current().text);
	if (kind() == TokenKind::Identifier) {
		if (isTypeQualifier(head) || addressSpaceFor(head)) {
			throw CompileError("a cast to a qualified, pointer or reference type is not supported");
		}
		// A cast to void evaluates the operand and discards it; the empty type stands
		// for void, which is what a Type holds until something sets it.
		if (head == "void") {
			nameTokens = 1;
			return Type();
		}
	}

	QualifiedName spelled;
	if (!peekQualifiedName(spelled)) {
		return std::nullopt;
	}
	const std::string name = peekResolved(nameTokens);

	Type type;
	if (!resolveTypeName(name, type)) {
		if (!_unit.findStruct(name)) {
			return std::nullopt;
		}
		type.namedType = name;
	}

	return type;
}

// `(T)operand`, where T names a scalar, vector or matrix type, a typedef, an
// enum or a struct, is the functional cast T(operand). Returns null when the
// parenthesis opens an expression, which includes one that starts with a functional
// cast, "(int(x) * 2)": a type name followed by anything but ")" is not a C-style cast.
ExpressionPtr Parser::parseCast() {
	const Token& first = lookahead(1);
	if (first.kind != TokenKind::Identifier && first.kind != TokenKind::ColonColon) {
		return nullptr;
	}

	// "(unsigned int)x" and "(const unsigned char)x": a run of integer specifier
	// words, which only a type can begin. A qualifier may come first.
	{
		size_t skip = 1;
		while (lookahead(skip).kind == TokenKind::Identifier
			&& (lookahead(skip).text == "const" || lookahead(skip).text == "volatile")) {
			++skip;
		}
		if (lookahead(skip).kind == TokenKind::Identifier && isIntegerSpecifier(lookahead(skip).text)) {
			const size_t open = _position;
			advance();
			Type type;
			parseSpecifierRun(type);
			if (at(TokenKind::Star) || at(TokenKind::Ampersand) || at(TokenKind::Less)) {
				throw CompileError("a cast to a pointer, reference or template type is not supported");
			}
			// "(int(x) * 2)": the type name starts a functional cast inside a parenthesised
			// expression, not a C-style cast, so the parenthesis opens an expression.
			if (!at(TokenKind::RParen)) {
				_position = open;
				return nullptr;
			}
			expect(TokenKind::RParen, "to close a cast");
			auto expression = std::make_unique<Expression>();
			expression->kind = ExpressionKind::Construct;
			expression->line = line();
			expression->constructType = type;
			const NestingScope scope(*this);
			expression->arguments.push_back(parseUnary());
			measure(*expression);
			return expression;
		}
	}

	// Read the name, which may be namespace-qualified, from just past the '('.
	const size_t open = _position;
	++_position;
	// (vec<T, N>)x and (make_unsigned_t<T>)x; a parenthesis holding more than the type, "(vec<float, 4>(a).x)",
	// is an expression.
	if (size_t vecTokens = 0; atLibraryVec(vecTokens) || atTraitType(vecTokens)) {
		const size_t atLine = line();
		Type target;
		try {
			target = parseType(false, false);
		} catch (...) {
			_position = open;
			throw;
		}
		if (!at(TokenKind::RParen)) {
			_position = open;
			return nullptr;
		}
		advance();
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Construct;
		expression->line = atLine;
		expression->constructType = target;
		const NestingScope scope(*this);
		expression->arguments.push_back(parseUnary());
		measure(*expression);
		return expression;
	}
	size_t nameTokens = 0;
	std::optional<Type> type;
	try {
		type = peekCastTargetName(nameTokens);
		// A variable or function of the name hides the struct or enum tag, so "(S) - 1" is
		// S minus one and not a cast ([basic.scope.hiding]); parseSizeof reads the same rule.
		if (type && (kind() == TokenKind::Identifier || at(TokenKind::ColonColon))) {
			size_t spelledTokens = 0;
			const std::string spelled = peekResolved(spelledTokens);
			Type probe;
			const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
				&& isLocal(spelled) && !resolveTypeName(spelled, probe);
			const bool memberHides = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
				&& memberHidesName(spelled);
			if (objectHidesTag(spelled) || memberHides || localHidesStruct) {
				type.reset();
			}
		}
	} catch (...) {
		_position = open;
		throw;
	}
	_position = open;
	if (!type) {
		return nullptr;
	}

	const Token& after = lookahead(1 + nameTokens);
	if (after.kind == TokenKind::Star || after.kind == TokenKind::Ampersand || after.kind == TokenKind::Less) {
		throw CompileError("a cast to a pointer, reference or template type is not supported");
	}
	if (after.kind != TokenKind::RParen) {
		return nullptr;
	}

	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::Construct;
	expression->line = line();
	expression->constructType = *type;
	advance();
	for (size_t i = 0; i < nameTokens; ++i) {
		advance();
	}
	advance();
	const NestingScope scope(*this);
	expression->arguments.push_back(parseUnary());
	measure(*expression);
	return expression;
}

// Unlike a C-style cast, a named cast cannot backtrack into an expression.
ExpressionPtr Parser::parseNamedCast() {
	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::Construct;
	expression->line = line();

	advance();
	advance();

	Type type;
	while (kind() == TokenKind::Identifier
		&& (current().text == "const" || current().text == "volatile")) {
		advance();
	}
	if (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text)) {
		parseSpecifierRun(type);
	} else {
		size_t nameTokens = 0;
		auto target = peekCastTargetName(nameTokens);
		if (!target) {
			if (nameTokens > 0 && lookahead(nameTokens).kind == TokenKind::Less) {
				throw CompileError("a cast to a pointer, reference or template type is not supported");
			}
			throw CompileError("expected a type name in \"static_cast<\", found \""
				+ std::string(current().text) + "\"");
		}
		type = *target;
		for (size_t i = 0; i < nameTokens; ++i) {
			advance();
		}
	}

	if (at(TokenKind::Star) || at(TokenKind::Ampersand) || at(TokenKind::Less)) {
		throw CompileError("a cast to a pointer, reference or template type is not supported");
	}
	expect(TokenKind::Greater, "to close the type of a static_cast");
	expect(TokenKind::LParen, "for the operand of a static_cast");
	{
		const NestingScope scope(*this);
		expression->arguments.push_back(parseExpression());
	}
	expect(TokenKind::RParen, "to close a static_cast");
	if (auto call = constructWithConstructor(type, expression->arguments, expression->line)) {
		return call;
	}
	expression->constructType = type;
	measure(*expression);
	return expression;
}

} // namespace mslc
