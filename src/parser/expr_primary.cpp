#include "parser/internal.h"

namespace mslc {

std::optional<Type> Parser::declaredPartType(const Expression& operand) const {
	if (operand.kind == ExpressionKind::Index && !operand.isDereference) {
		std::optional<Type> base = declaredPartType(*operand.left);
		if (!base || (base->isPointer && base->arrayLength)) {
			return std::nullopt;
		}
		if (base->arrayLength) {
			base->dropOuterDimension();
		} else if (base->isPointer) {
			base->isPointer = false;
			base->dropOuterDimension();
		} else {
			return std::nullopt;
		}
		return base;
	}
	if (operand.kind != ExpressionKind::Member) {
		return declaredObjectType(operand);
	}
	const std::optional<Type> owner = declaredPartType(*operand.left);
	if (!owner || owner->arrayLength || owner->namedType.empty() || owner->isPointer != operand.isArrow) {
		return std::nullopt;
	}
	const StructDecl* decl = _unit.findStruct(owner->namedType);
	if (!decl) {
		return std::nullopt;
	}
	for (const StructField& field: decl->fields) {
		if (field.name == operand.memberName) {
			return field.type;
		}
	}
	return std::nullopt;
}

ExpressionPtr Parser::foldStdArrayCall(ExpressionPtr& receiver) {
	if (receiver->kind != ExpressionKind::Member || receiver->isArrow || lookahead().kind != TokenKind::RParen) {
		return nullptr;
	}
	const std::string& name = receiver->memberName;
	if (name != "size" && name != "max_size" && name != "empty" && name != "front" && name != "back") {
		return nullptr;
	}
	const std::optional<Type> declared = declaredPartType(*receiver->left);
	if (!declared || !declared->isStdArray || !declared->arrayLength || declared->isPointer) {
		return nullptr;
	}
	const size_t atLine = receiver->line;
	advance();
	advance();
	auto folded = std::make_unique<Expression>();
	folded->line = atLine;
	if (name == "empty") {
		folded->kind = ExpressionKind::BoolLiteral;
		folded->boolValue = false;
		return folded;
	}
	folded->kind = ExpressionKind::IntLiteral;
	folded->intKind = ScalarKind::ULong;
	folded->isSizeof = true;
	folded->intValue = name == "back" ? *declared->arrayLength - 1 : *declared->arrayLength;
	if (name == "size" || name == "max_size") {
		return folded;
	}
	// front() and back() are the elements themselves, as places: a[0] and a[N - 1].
	folded->intValue = name == "back" ? *declared->arrayLength - 1 : 0;
	folded->isSizeof = false;
	folded->intKind = ScalarKind::Int;
	auto index = std::make_unique<Expression>();
	index->kind = ExpressionKind::Index;
	index->line = atLine;
	index->left = std::move(receiver->left);
	index->arguments.push_back(std::move(folded));
	measure(*index);
	return index;
}

ExpressionPtr Parser::parsePostfix() {
	auto expression = parsePrimary();

	while (true) {
		if (at(TokenKind::Increment) || at(TokenKind::Decrement)) {
			auto step = std::make_unique<Expression>();
			step->kind = ExpressionKind::Unary;
			step->line = expression->line;
			step->unaryOperator = at(TokenKind::Increment)
				? UnaryOperator::PostIncrement : UnaryOperator::PostDecrement;
			advance();
			step->left = std::move(expression);
			measure(*step);
			return step;
		}

		if (at(TokenKind::LBracket)) {
			advance();
			auto index = std::make_unique<Expression>();
			index->kind = ExpressionKind::Index;
			index->line = expression->line;
			index->left = std::move(expression);
			index->arguments.push_back(parseExpression());
			expect(TokenKind::RBracket, "to close an index");
			measure(*index);
			expression = std::move(index);
			continue;
		}

		// "->" reaches a member of what a pointer points at, which for the
		// emitter is the same member access the pointer itself would give.
		if (at(TokenKind::Dot) || at(TokenKind::Arrow)) {
			const std::string op(advance().text);
			// "x.template f<int>(a)" names a member template; C++ requires the keyword when x depends on a
			// template parameter, and Apple takes it anywhere the name is one.
			const bool keyword = atKeyword("template") && lookahead().kind == TokenKind::Identifier;
			if (keyword) {
				advance();
			}
			if (atKeyword("operator")) {
				throw CompileError("calling an operator with member syntax is not lowered; write s[i]");
			}
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a member name after '" + op + "'");
			}
			auto member = std::make_unique<Expression>();
			member->kind = ExpressionKind::Member;
			member->isArrow = op == "->";
			member->line = expression->line;
			member->left = std::move(expression);
			member->memberName = std::string(advance().text);
			parseMemberTemplateArguments(*member, keyword);
			measure(*member);
			expression = std::move(member);
			continue;
		}

		if (at(TokenKind::LParen)) {
			if (ExpressionPtr folded = foldStdArrayCall(expression)) {
				expression = std::move(folded);
				continue;
			}
			const auto hasName = [&](const std::set<std::string>& set) {
				return set.count(expression->name) || std::any_of(expression->overloadNames.begin(),
					expression->overloadNames.end(), [&](const std::string& n) { return set.count(n) != 0; });
			};
			if (expression->kind == ExpressionKind::Identifier && !expression->hasTemplateArguments
				&& (expression->fileScope || !isLocal(expression->name))
				&& hasName(_entryTemplateNames) && !hasName(_unit.functionTemplates) && !hasName(_helperNames)) {
				throw CompileError("cannot call the entry point \"" + expression->name + "\"");
			}
			advance();
			auto call = std::make_unique<Expression>();
			call->kind = ExpressionKind::Call;
			call->line = expression->line;
			call->left = std::move(expression);
			call->arguments = parseArgumentList("to close an argument list");
			measure(*call);
			expression = std::move(call);
			continue;
		}

		return expression;
	}
}

// "<float, 3>" after the name of a member function template, in a call: the "<" is the current token. Only a
// name some struct declares as a member template is tried, and only a list closed by ">(" is taken, so a
// comparison of a field of that name is left alone.
void Parser::parseMemberTemplateArguments(Expression& member, bool keyword) {
	member.templateKeyword = keyword;
	if (keyword && !_memberTemplateNames.count(member.memberName)) {
		throw CompileError("\"" + member.memberName + "\" follows the 'template' keyword, and no struct declares a "
			"member function template of that name");
	}
	// "x.template f(a)" leaves the arguments to deduction: with no "<" the call below is read as any other.
	if (!at(TokenKind::Less) || !_memberTemplateNames.count(member.memberName)) {
		return;
	}
	const size_t saved = _position;
	bool taken = false;
	try {
		advance();
		std::vector<TemplateArgument> arguments;
		while (!at(TokenKind::Greater)) {
			arguments.push_back(parseTemplateArgument());
			if (!match(TokenKind::Comma)) {
				break;
			}
		}
		if (at(TokenKind::Greater) && lookahead().kind == TokenKind::LParen) {
			advance();
			member.templateArguments = std::move(arguments);
			taken = true;
		}
	} catch (const ClassTemplateError&) {
		throw;
	} catch (const CompileError&) {
	}
	// Inside a template body Apple rejects "x.f<int>(a)" when x depends on a template parameter, and takes it when
	// x does not. mslc takes it for "this" and for a local or parameter whose declared type names no template
	// parameter; any other receiver is taken as dependent, which over-refuses a few valid sources loudly.
	if (taken && !keyword && (_classDepth > 0 || _inInstance) && !receiverIndependent(*member.left)) {
		throw CompileError("explicit template arguments on \"" + member.memberName + "\" inside a template need the "
			"keyword unless the receiver is known not to depend on a template parameter: write \".template "
			+ member.memberName + "<...>(...)\" (Apple requires it when the receiver depends on one; mslc takes the "
			"bare form only where the receiver's type provably names no template parameter, so a dependent "
			"receiver, an auto local or a receiver inside a class template that is not a concrete local needs the "
			"keyword)");
	}
	if (taken) {
		return;
	}
	if (keyword) {
		throw CompileError("\"template " + member.memberName + "\" is followed by \"<\", which is not a list of "
			"explicit template arguments closed by \">(\"");
	}
	_position = saved;
}

std::vector<ExpressionPtr> Parser::parseArgumentList(const char* closing) {
	std::vector<ExpressionPtr> arguments;

	while (!at(TokenKind::RParen) && !at(TokenKind::EndOfFile)) {
		arguments.push_back(parseAssignment());

		if (!match(TokenKind::Comma)) {
			break;
		}

		// A trailing comma is not a list with an empty last element; Apple's
		// compiler reports "expected expression" for it. Accepting it here would
		// mean mslc compiles a source the GPU compiler rejects.
		if (at(TokenKind::RParen)) {
			throw CompileError("expected an expression after ',' in an argument list");
		}
	}

	expect(TokenKind::RParen, closing);
	return arguments;
}

// The type of an integer literal, as C gives it and as Apple's compiler does.
// An unsuffixed decimal is the first of int and long that holds it, where hex
// and octal go on through uint and ulong; a decimal never becomes unsigned
// without a `u`, and one too large for a long wraps, which is what Apple
// compiles it to.
static ScalarKind integerLiteralKind(const Token& literal) {
	if (literal.integerDigitsInvalid) {
		throw CompileError("invalid digit in integer constant \"" + std::string(literal.text) + "\"");
	}

	if (literal.integerSuffixInvalid) {
		size_t digitsEnd = literal.text.size();
		while (digitsEnd > 0 && (std::string_view("uUlL").find(literal.text[digitsEnd - 1]) != std::string_view::npos)) {
			--digitsEnd;
		}
		throw CompileError("invalid suffix \"" + std::string(literal.text.substr(digitsEnd))
			+ "\" on integer constant");
	}

	if (literal.integerOverflows) {
		throw CompileError("integer literal is too large to be represented in any integer type");
	}

	const uint64_t value = literal.integerValue;
	if (literal.integerIsUnsigned) {
		return literal.integerLongCount == 0 && value <= UINT_MAX ? ScalarKind::UInt : ScalarKind::ULong;
	}

	// An l is a long, and a long long is one here too. Apple wraps a decimal or
	// a long long that is too large rather than making it unsigned, and only a
	// hex or octal l goes on to ulong.
	if (literal.integerLongCount > 0) {
		const bool wraps = literal.integerIsDecimal || literal.integerLongCount == 2;
		return wraps || value <= INT64_MAX ? ScalarKind::Long : ScalarKind::ULong;
	}

	if (value <= INT_MAX) {
		return ScalarKind::Int;
	}

	// A decimal from 2^63 to 2^64 - 1 is a long holding the wrapped bits, as in
	// Apple's compiler, which says nothing about a bare literal and only warns
	// when it is narrowed into a variable. C would make it unsigned.
	if (literal.integerIsDecimal) {
		return ScalarKind::Long;
	}

	if (value <= UINT_MAX) {
		return ScalarKind::UInt;
	}

	return value <= INT64_MAX ? ScalarKind::Long : ScalarKind::ULong;
}

ExpressionPtr Parser::parsePrimary() {
	// mslc lowers no string type, so a string in expression position is a
	// rejection. Naming it as one is the whole point: read as a name, it
	// reported a failed lookup for an identifier spelled with quotes.
	// Deliberate limit (known-issues.md): char pointers, char arrays and
	// bool-from-pointer have no lowered representation.
	static const char* const kStringLimit = "mslc has no string type, so char pointers, "
		"char arrays and bool-from-pointer are not lowered";
	if (at(TokenKind::StringLiteral)) {
		throw CompileError(std::string("a string literal is not lowered; ") + kStringLimit);
	}

	// L"x", u8"x", R"(x)": the lexer reads the prefix as an identifier glued to a string.
	if (kind() == TokenKind::Identifier && lookahead().kind == TokenKind::StringLiteral
		&& current().text.data() + current().text.size() == lookahead().text.data()) {
		static const std::string_view prefixes[] = {"L", "u", "U", "u8", "R", "LR", "uR", "UR", "u8R"};
		for (const std::string_view prefix : prefixes) {
			if (current().text == prefix) {
				throw CompileError("a prefixed string literal is not lowered; " + std::string(kStringLimit));
			}
		}
	}

	if (ExpressionPtr trait = tryParseTraitValue()) {
		return trait;
	}

	if (kind() == TokenKind::Identifier && lookahead().kind == TokenKind::Less) {
		if (atKeyword("static_cast")) {
			return parseNamedCast();
		}
		if (atKeyword("reinterpret_cast") || atKeyword("const_cast")) {
			throw CompileError(std::string(current().text) + " is not supported; "
				"use static_cast or a functional cast for a value conversion");
		}
	}

	if (at(TokenKind::IntegerLiteral)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::IntLiteral;
		expression->line = line();
		const Token literal = advance();
		expression->intValue = literal.integerValue;
		expression->intKind = integerLiteralKind(literal);
		return expression;
	}

	if (at(TokenKind::FloatLiteral)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::FloatLiteral;
		expression->line = line();
		const Token literal = advance();
		expression->floatValue = literal.floatValue;
		expression->floatIsHalf = literal.floatIsHalf;
		return expression;
	}

	if (atKeyword("true") || atKeyword("false")) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::BoolLiteral;
		expression->line = line();
		expression->boolValue = atKeyword("true");
		advance();
		return expression;
	}

	if (at(TokenKind::LParen)) {
		if (lookahead().kind == TokenKind::LBrace) {
			throw CompileError("a statement expression \"({ ... })\" is lowered only as a statement of its own, "
				"\"({ ... });\"; one in an expression is not lowered");
		}
		advance();
		const RestoreFlag freshExpression(_noGreater, false);
		auto expression = parseExpression();
		expect(TokenKind::RParen, "to close a parenthesised expression");
		return expression;
	}

	if (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text)) {
		// unsigned(x) and short(x) are functional casts; a run of more words is
		// not one, as in C++, and Apple rejects it.
		Type type;
		if (parseSpecifierRun(type) > 1) {
			throw CompileError("a functional cast takes a single-word type name; write (" + typeName(type)
				+ ")x instead");
		}
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Construct;
		expression->line = line();
		expression->constructType = type;
		if (!at(TokenKind::LParen)) {
			throw CompileError("expected '(' after type \"" + typeName(type) + "\"");
		}
		advance();
		expression->arguments = parseArgumentList("to close a constructor's argument list");
		measure(*expression);
		return expression;
	}

	if (size_t arrayTokens = 0; (kind() == TokenKind::Identifier || at(TokenKind::ColonColon))
		&& !isLocal(std::string(current().text)) && atStdArray(arrayTokens)) {
		const size_t atLine = line();
		const Type type = parseType(false, false);
		if (!at(TokenKind::LBrace)) {
			throw CompileError("array<T, N> in an expression is lowered as a braced list, array<T, N>{...}");
		}
		ExpressionPtr list = parseInitializerList();
		list->constructType = type;
		list->line = atLine;
		return list;
	}

	if (size_t asTypeTokens = 0; atAsType(asTypeTokens)) {
		return parseAsType(asTypeTokens);
	}

	// vec<T, N>(...) constructs the vector, as float4(...) does.
	if (size_t vecTokens = 0; (kind() == TokenKind::Identifier || at(TokenKind::ColonColon))
		&& !isLocal(std::string(current().text)) && atLibraryVec(vecTokens)) {
		const size_t atLine = line();
		const Type type = parseType(false, false);
		if (!at(TokenKind::LParen)) {
			throw CompileError("expected '(' after type \"" + typeName(type) + "\"");
		}
		advance();
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Construct;
		expression->line = atLine;
		expression->constructType = type;
		expression->arguments = parseArgumentList("to close a constructor's argument list");
		measure(*expression);
		return expression;
	}

	if (kind() == TokenKind::Identifier || at(TokenKind::ColonColon)) {
		// A name written with namespaces is read whole; a plain one is looked up
		// unless a parameter or local of that name is in scope.
		size_t spelledTokens = 1;
		QualifiedName spelled;
		// "::k" skips the local k: the identifier carries that, since both are named k.
		const bool fileScoped = peekQualifiedName(spelled) && spelled.global && spelled.parts.size() == 1
			&& isLocal(spelled.parts[0]);
		if (_memberFields && kind() == TokenKind::Identifier && !blockName(std::string(current().text))
			&& _memberFields->count(std::string(current().text)) && isLocal("this")
			&& lookahead().kind != TokenKind::ColonColon) {
			const size_t atLine = line();
			auto member = makeThisMember(std::string(advance().text), atLine);
			return member;
		}
		// A member function named bare calls that function on this, or the static
		// one of the struct.
		if (_memberFields && kind() == TokenKind::Identifier && !blockName(std::string(current().text))
			&& (lookahead().kind == TokenKind::LParen
				|| (lookahead().kind == TokenKind::Less && _memberTemplateNames.count(std::string(current().text))))) {
			const auto function = _memberFunctions.find(std::string(current().text));
			if (function != _memberFunctions.end()) {
				const size_t atLine = line();
				const std::string name(advance().text);
				if (function->second) {
					ExpressionPtr staticCall = makeIdentifier(_memberStruct + "::" + name, atLine);
					if (at(TokenKind::Less)) {
						advance();
						staticCall->hasTemplateArguments = true;
						while (!at(TokenKind::Greater)) {
							staticCall->templateArguments.push_back(parseTemplateArgument());
							if (!match(TokenKind::Comma)) {
								break;
							}
						}
						expect(TokenKind::Greater, "to close the template arguments");
					}
					return staticCall;
				}
				ExpressionPtr member = makeThisMember(name, atLine);
				parseMemberTemplateArguments(*member);
				return member;
			}
		}
		// A local hides a namespace's name only where it is not followed by "::".
		std::vector<std::string> alternatives;
		const std::string resolved = kind() == TokenKind::Identifier && isLocal(std::string(current().text))
			&& lookahead().kind != TokenKind::ColonColon ? std::string(current().text)
			: peekResolved(spelledTokens, &alternatives);
		const auto consumeName = [&] {
			for (size_t i = 0; i < spelledTokens; ++i) {
				advance();
			}
		};
		const std::string_view text = resolved;

		Type type;
		const bool tagHidden = objectHidesTag(resolved);
		if (!tagHidden && resolveTypeName(text, type) && type.namedType.empty()) {
			// A type name in expression position constructs a value: float(x),
			// float3(0), float4(a, b, c, 1). T(...) is a constructor call, and the
			// parenthesised part is a list of arguments; (T)x is parseCast's.
			consumeName();

			if (type.isStdArray) {
				if (!at(TokenKind::LBrace)) {
					throw CompileError("array<T, N> in an expression is lowered as a braced list, " + std::string(text) + "{...}");
				}
				ExpressionPtr list = parseInitializerList();
				list->constructType = type;
				return list;
			}
			if (!at(TokenKind::LParen)) {
				throw CompileError("expected '(' after type \"" + std::string(text) + "\"");
			}

			advance();
			auto expression = std::make_unique<Expression>();
			expression->kind = ExpressionKind::Construct;
			expression->line = line();
			expression->constructType = type;
			expression->arguments = parseArgumentList("to close a constructor's argument list");
			measure(*expression);
			return expression;
		}

		// A struct's name and a braced list is a value of that struct: S{a, b} and
		// S{.field = a}. It is spelled in a return, an argument or an initialiser.
		// A typedef of a struct names the same struct: T{a, b} after "typedef S T;".
		std::string braced = resolved;
		if (Type aliased; resolveTypeName(text, aliased) && !aliased.namedType.empty()
			&& _unit.findStruct(aliased.namedType)) {
			braced = aliased.namedType;
		}
		if (!tagHidden && (fileScoped || !isLocal(resolved)) && _unit.findStruct(braced) && lookahead(spelledTokens).kind == TokenKind::LBrace) {
			consumeName();
			ExpressionPtr list = parseInitializerList();
			Type type;
			type.namedType = braced;
			list->constructType = type;
			list->namesConcreteStruct = resolved.rfind("<template ", 0) != 0;
			return list;
		}

		// A struct's name and an argument list call its constructor.
		if (!tagHidden && (fileScoped || !isLocal(resolved)) && _constructors.count(braced) && lookahead(spelledTokens).kind == TokenKind::LParen) {
			const size_t atLine = line();
			consumeName();
			advance();
			Type type;
			type.namedType = braced;
			std::vector<ExpressionPtr> arguments = parseArgumentList("to close a constructor's argument list");
			ExpressionPtr call = constructWithConstructor(type, arguments, atLine);
			if (call) {
				call->namesConcreteStruct = resolved.rfind("<template ", 0) != 0;
			}
			if (!call) {
				throw CompileError("\"" + braced + "\" has a constructor with nothing to do, and a value of it is not lowered");
			}
			return call;
		}

		// INFINITY, HUGE_VALF, HUGE_VALH and NAN expand to these compiler builtins: each is
		// the one value its name says, so it is the float constant.
		if ((resolved == "__builtin_inff" || resolved == "__builtin_huge_valf" || resolved == "__builtin_huge_valh"
			|| resolved == "__builtin_nanf") && lookahead().kind == TokenKind::LParen) {
			const bool wantsString = resolved == "__builtin_nanf";
			if (wantsString ? !(lookahead(2).kind == TokenKind::StringLiteral && lookahead(3).kind == TokenKind::RParen)
				: lookahead(2).kind != TokenKind::RParen) {
				throw CompileError("\"" + resolved + "\" takes " + (wantsString ? "a string" : "no arguments"));
			}
			auto expression = std::make_unique<Expression>();
			expression->line = line();
			expression->kind = ExpressionKind::FloatLiteral;
			expression->floatValue = wantsString ? std::numeric_limits<double>::quiet_NaN()
				: std::numeric_limits<double>::infinity();
			expression->floatIsHalf = resolved == "__builtin_huge_valh";
			consumeName();
			advance();
			if (wantsString) {
				advance();
			}
			advance();
			return expression;
		}

		// An enumerator is the integer it was declared with.
		const auto constant = _enumConstants.find(resolved);
		if (constant != _enumConstants.end()) {
			auto expression = std::make_unique<Expression>();
			expression->line = line();
			consumeName();
			expression->kind = ExpressionKind::IntLiteral;
			const int64_t value = constant->second.value;
			if (constant->second.kind == ScalarKind::Bool) {
				// A bool template parameter's value.
				expression->kind = ExpressionKind::BoolLiteral;
				expression->boolValue = value != 0;
				return expression;
			}
			expression->intKind = constant->second.kind;
			expression->enumName = constant->second.owner;
			expression->intValue = static_cast<uint64_t>(value < 0 ? -value : value);
			if (value >= 0) {
				return expression;
			}

			auto negated = std::make_unique<Expression>();
			negated->kind = ExpressionKind::Unary;
			negated->line = expression->line;
			negated->unaryOperator = UnaryOperator::Negate;
			negated->enumName = expression->enumName;
			negated->left = std::move(expression);
			measure(*negated);
			return negated;
		}

		// "f<float, 3>(x)": the "<" after a function template's name opens its arguments. A name that
		// a block declares (a local, a using declaration) is not looked up as the template.
		const bool blocked = spelledTokens == 1 && blockName(std::string(current().text));
		const auto matchesSet = [&](const std::set<std::string>& set) {
			return set.count(resolved) || std::any_of(alternatives.begin(), alternatives.end(),
				[&](const std::string& each) { return set.count(each) != 0; });
		};
		const bool templateId = lookahead(spelledTokens).kind == TokenKind::Less && !blocked && matchesSet(_unit.functionTemplates);
		if (!templateId && lookahead(spelledTokens).kind == TokenKind::Less && !blocked && matchesSet(_entryTemplateNames)) {
			throw CompileError("cannot call the entry point \"" + resolved + "\"");
		}

		// A name that is also an MSL builtin is a plain identifier here. Whether
		// it is the builtin or a parameter that shadows one is a question about
		// scope, which the parser has no answer for, so the emitter decides.
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Identifier;
		expression->line = line();
		expression->name = resolved;
		expression->overloadNames = alternatives;
		expression->templateDependent = spelledTokens == 1 && namesDependentParameter(std::string(current().text));
		// An unqualified name in a call also finds the functions of its arguments' namespaces.
		expression->unqualifiedCall = spelledTokens == 1 && lookahead(spelledTokens).kind == TokenKind::LParen
			&& !isLocal(std::string(current().text));
		expression->fileScope = fileScoped;
		// A qualified call is looked up at the scope it WROTE, not at the scope the member
		// it found happens to live in: "B::clamp" can resolve to "A::clamp" through a using
		// declaration or directive in B, but it is still B's lookup that also reaches the
		// library when B brings metal in too (directly, as namespace metal itself, or
		// transitively), so collectMembers walks from the written scope.
		if (spelled.parts.size() > 1) {
			const std::string scope = namespaceOf(spelled, spelled.parts.size() - 1);
			if (!scope.empty()) {
				std::set<std::string> seen, found;
				collectMembers(scope, spelled.parts.back(), seen, found, true, &expression->stdlibBeside);
			}
		} else if (spelled.global) {
			collectStdlibOwn("", spelled.parts.back(), nullptr, &expression->stdlibBeside);
		}
		if (spelledTokens == 1 && kind() == TokenKind::Identifier && !blockName(std::string(current().text))) {
			unqualifiedFound(std::string(current().text), &expression->stdlibBeside);
		}
		consumeName();
		if (templateId) {
			advance();
			expression->hasTemplateArguments = true;
			while (!at(TokenKind::Greater)) {
				expression->templateArguments.push_back(parseTemplateArgument());
				if (!match(TokenKind::Comma)) {
					break;
				}
			}
			expect(TokenKind::Greater, "to close the template arguments");
			expression->unqualifiedCall = spelledTokens == 1 && at(TokenKind::LParen) && !isLocal(std::string(spelled.parts.back()));
		}
		return expression;
	}

	throw CompileError("unexpected " + std::string(tokenKindName(kind())) + " \""
		+ std::string(current().text) + "\" in an expression");
}

} // namespace mslc
