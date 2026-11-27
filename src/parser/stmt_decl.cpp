#include "parser/internal.h"

namespace mslc {

// The initialiser of an auto local, which the type is deduced from: "= value",
// or a single value in parentheses or braces. "= {v}" is a list, which Apple
// deduces as a different type, so it is left as one for the lowering to refuse.
void Parser::parseAutoInitializer(VariableDeclaration& declaration) {
	const std::string quoted = "\"" + declaration.name + "\"";
	if (match(TokenKind::Assign)) {
		const size_t from = _position;
		declaration.initializer = parseInitializer();
		noteAutoConstruction(declaration.name, from);
	} else if (match(TokenKind::LParen)) {
		std::vector<ExpressionPtr> arguments = parseArgumentList("to close the initialiser of an auto variable");
		if (arguments.size() != 1) {
			throw CompileError("the auto variable " + quoted + " needs exactly one value to deduce its type from");
		}
		declaration.initializer = std::move(arguments.front());
	} else if (at(TokenKind::LBrace)) {
		ExpressionPtr list = parseInitializerList();
		if (list->elements.size() != 1 || !list->elements.front().fieldName.empty()) {
			throw CompileError("the auto variable " + quoted + " needs exactly one value to deduce its type from");
		}
		declaration.initializer = std::move(list->elements.front().value);
	} else {
		throw CompileError("the variable " + quoted + " is declared with auto and needs an initialiser to deduce its type from");
	}
}

// True when the parenthesised list ahead reads as the parameters of a function
// declaration: every comma group is a type, then "(", an optional name and ")"
// ("float(a)"), as C++ resolves "float3 v(float(a));". Empty parentheses are
// not matched here. The position is restored.
bool Parser::parensDeclareFunction() {
	const size_t start = _position;
	advance();
	bool declares = true;
	do {
		if (atKeyword("const")) {
			advance();
		}
		size_t typeTokens = 0;
		Type probe;
		const bool named = kind() == TokenKind::Identifier || at(TokenKind::ColonColon);
		const std::string name = named ? peekResolved(typeTokens) : std::string();
		const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
			&& isLocal(name);
		if (!named || objectHidesTag(name)
			|| !(resolveTypeName(name, probe) || (_unit.findStruct(name) != nullptr && !localHidesStruct))
			|| lookahead(typeTokens).kind != TokenKind::LParen) {
			declares = false;
			break;
		}
		for (size_t i = 0; i <= typeTokens; ++i) {
			advance();
		}
		while (at(TokenKind::Star) || at(TokenKind::Ampersand)) {
			advance();
		}
		size_t nested = 0;
		while (at(TokenKind::LParen)) {
			++nested;
			advance();
		}
		if (kind() == TokenKind::Identifier) {
			advance();
		}
		while (at(TokenKind::LBracket) || (nested > 0 && at(TokenKind::RParen))) {
			if (at(TokenKind::RParen)) {
				--nested;
				advance();
				continue;
			}
			int depth = 0;
			do {
				depth += at(TokenKind::LBracket) ? 1 : at(TokenKind::RBracket) ? -1 : 0;
				advance();
			} while (depth > 0 && !at(TokenKind::EndOfFile));
		}
		const bool closed = at(TokenKind::RParen);
		if (closed) {
			advance();
		}
		if (!closed || !(at(TokenKind::Comma) || at(TokenKind::RParen))) {
			declares = false;
			break;
		}
	} while (match(TokenKind::Comma));
	_position = start;
	return declares;
}

void Parser::rejectParameterListInitializer(const VariableDeclaration& declaration) {
	if (!at(TokenKind::LParen) || !parensDeclareFunction()) {
		return;
	}
	Type plain = declaration.type;
	plain.isConst = false;
	throw CompileError("\"" + declaration.name + "\" is followed by a parameter list, so C++ and "
		"xcrun metal declare a function here, not a variable (the most vexing parse), and mslc does not "
		"lower block-scope function declarations; write braces, \"" + declaration.name + "{...}\", or "
		"\"= " + (plain.isAuto ? std::string("value") : typeName(plain) + "(...)") + "\" to construct the variable");
}

// The initialiser a local may have after its name: "= value", a braced list,
// or constructor arguments. A const local has to have one.
void Parser::parseLocalInitializer(VariableDeclaration& declaration) {
	rejectParameterListInitializer(declaration);

	if (declaration.type.isAuto) {
		parseAutoInitializer(declaration);
		return;
	}

	if (declaration.type.isConst && !declaration.type.isPointer && !at(TokenKind::Assign)
		&& !at(TokenKind::LParen) && !at(TokenKind::LBracket) && !at(TokenKind::LBrace)) {
		throw CompileError("the const variable \"" + declaration.name + "\" needs an initialiser");
	}

	if (match(TokenKind::Assign)) {
		declaration.initializer = parseInitializer();
	} else if (at(TokenKind::LBrace)) {
		declaration.initializer = parseInitializerList();
	} else if (at(TokenKind::LParen)) {
		// Direct initialisation, float3 specularTerm(0); . The name comes
		// first here, so the list after it constructs the variable's own
		// type rather than calling it, and the two spellings are the same
		// value. Metal has no other form: "float3(0) specularTerm;" is a
		// parse error there, which xcrun metal confirms.
		advance();
		const size_t atLine = line();
		std::vector<ExpressionPtr> arguments = parseArgumentList("to close a constructor's argument list");
		declaration.initializer = constructWithConstructor(declaration.type, arguments, atLine);
		if (!declaration.initializer) {
			auto construct = std::make_unique<Expression>();
			construct->kind = ExpressionKind::Construct;
			construct->line = atLine;
			construct->constructType = declaration.type;
			construct->arguments = std::move(arguments);
			measure(*construct);
			declaration.initializer = std::move(construct);
		}
	} else {
		std::vector<ExpressionPtr> none;
		declaration.initializer = constructWithConstructor(declaration.type, none, line());
	}
}

void Parser::appendStatement(std::vector<StatementPtr>& out) {
	out.push_back(parseStatement());
	for (StatementPtr& tail: _declaratorTail) {
		out.push_back(std::move(tail));
	}
	_declaratorTail.clear();
}

StatementPtr Parser::parseBody(bool sharesEnclosingScope) {
	if (at(TokenKind::LBrace)) {
		return parseCompoundStatement(sharesEnclosingScope);
	}

	const NestingScope scope(*this);
	const LocalScope locals(*this);
	_blocks.back().sharesEnclosingScope = sharesEnclosingScope;
	StatementPtr body = parseStatement();
	if (_declaratorTail.empty() && body->kind != StatementKind::DeclarationStatement) {
		return body;
	}

	// "if (c) int a, b;" is one declaration in a scope of its own, and so is a
	// lone one: without a block its name would replace the enclosing one.
	auto block = std::make_unique<Statement>();
	block->kind = StatementKind::Compound;
	block->line = body->line;
	block->children.push_back(std::move(body));
	for (StatementPtr& tail: _declaratorTail) {
		block->children.push_back(std::move(tail));
	}
	_declaratorTail.clear();
	return block;
}

StatementPtr Parser::parseCompoundStatement(bool sharesEnclosingScope) {
	const NestingScope scope(*this);
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Compound;
	statement->line = line();
	const LocalScope locals(*this);
	_blocks.back().sharesEnclosingScope = sharesEnclosingScope;

	expect(TokenKind::LBrace, "to open a block");

	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		appendStatement(statement->children);
	}

	expect(TokenKind::RBrace, "to close a block");

	return statement;
}

StatementPtr Parser::parseStatement() {
	auto statement = std::make_unique<Statement>();
	statement->line = line();

	if (at(TokenKind::LBrace)) {
		return parseCompoundStatement();
	}

	// A GNU statement expression that is a statement of its own, "({ ... });", which Apple's compiler takes as
	// clang does: its value is dropped, so it runs as the block it holds, in a scope of its own, and a break,
	// continue or return in it acts as one in that block would. One in an expression is refused (parsePrimary), even where its value is dropped.
	if (at(TokenKind::LParen) && lookahead().kind == TokenKind::LBrace) {
		size_t depth = 0;
		size_t close = 0;
		for (size_t k = 0;; ++k) {
			const TokenKind tokenKind = lookahead(k).kind;
			if (tokenKind == TokenKind::EndOfFile) {
				break;
			}
			if (tokenKind == TokenKind::LParen || tokenKind == TokenKind::LBrace || tokenKind == TokenKind::LBracket) {
				++depth;
			} else if (tokenKind == TokenKind::RParen || tokenKind == TokenKind::RBrace
				|| tokenKind == TokenKind::RBracket) {
				if (--depth == 0) {
					close = k;
					break;
				}
			}
		}
		if (close && lookahead(close).kind == TokenKind::RParen && lookahead(close - 1).kind == TokenKind::RBrace
			&& lookahead(close + 1).kind == TokenKind::Semicolon) {
			advance();
			StatementPtr block = parseCompoundStatement();
			expect(TokenKind::RParen, "to close a statement expression");
			expect(TokenKind::Semicolon, "after a statement expression");
			return block;
		}
	}

	if (at(TokenKind::Semicolon)) {
		advance();
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
	}

	// parseSwitchStatement takes the labels that sit directly in a switch body, so a label
	// that reaches here is outside any switch or nested in a compound, loop or if within one.
	// The second form (Duff's device) would need OpSwitch to branch into the middle of a
	// structured construct, which SPIR-V does not allow.
	if (atKeyword("case") || atKeyword("default")) {
		const std::string label = atKeyword("case") ? "case" : "default";
		if (_switchDepth == 0) {
			throw CompileError("'" + label + "' label is outside a switch statement");
		}
		throw CompileError("a '" + label + "' label nested inside a compound statement, loop or if within a switch "
			"body is not supported: put every label directly in the switch body");
	}

	if (matchIdentifier("static_assert")) {
		statement->refusal = parseStaticAssert(true);
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
	}

	if (atKeyword("namespace") && lookahead().kind == TokenKind::Identifier && lookahead(2).kind == TokenKind::Assign) {
		advance();
		parseNamespaceAlias();
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
	}

	// "enum Mode m;" declares a local of an enum declared earlier; anything else
	// that starts with "enum" would declare a type.
	const bool elaboratedEnumLocal = atKeyword("enum") && lookahead().kind == TokenKind::Identifier
		&& _enumTypes.count(std::string(lookahead().text)) && lookahead(2).kind == TokenKind::Identifier;
	if (atKeyword("typedef") || (atKeyword("enum") && !elaboratedEnumLocal)) {
		throw CompileError("\"" + std::string(current().text) + "\" inside a function is not supported; "
			"declare it at file scope");
	}

	// "using namespace N;" and "using N::x;" hold for the rest of the block.
	if (matchIdentifier("using")) {
		parseUsing();
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
	}

	if (atKeyword("if")) {
		return parseIfStatement();
	}

	if (atKeyword("for")) {
		return parseForStatement();
	}

	if (atKeyword("while")) {
		return parseWhileStatement();
	}

	if (atKeyword("do")) {
		return parseDoWhileStatement();
	}

	if (atKeyword("switch")) {
		return parseSwitchStatement();
	}

	if (atKeyword("return")) {
		return parseReturnStatement();
	}

	QualifiedName discardName;
	if (isDiscardFragment(discardName)) {
		statement->kind = StatementKind::Discard;
		for (size_t i = 0; i < discardName.tokens; ++i) {
			advance();
		}
		expect(TokenKind::LParen, "to open the call \"discard_fragment()\"");
		expect(TokenKind::RParen, "to close the call \"discard_fragment()\", which takes no arguments");
		expect(TokenKind::Semicolon, "after \"discard_fragment()\"");
		return statement;
	}

	if (atKeyword("break") || atKeyword("continue")) {
		const std::string keyword(advance().text);
		statement->kind = keyword == "break" ? StatementKind::Break : StatementKind::Continue;
		expect(TokenKind::Semicolon, "after a jump statement");
		return statement;
	}

	// The one attribute a statement carries is the fall-through marker, which
	// lowers to nothing: a case's block already branches to the next, and the
	// attribute only tells the reader the fall-through is meant.
	// Apple also takes [[clang::fallthrough]]; only [[::clang::fallthrough]] is an error there. It
	// accepts further spellings ([[gnu::fallthrough]], [[__fallthrough__]]) that are not recognised here.
	const size_t nameAt = at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket
		&& lookahead(2).kind == TokenKind::Identifier && lookahead(2).text == "clang"
		&& lookahead(3).kind == TokenKind::ColonColon ? 4 : 2;
	if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket
		&& lookahead(nameAt).kind == TokenKind::Identifier && lookahead(nameAt).text == "fallthrough"
		&& lookahead(nameAt + 1).kind == TokenKind::RBracket && lookahead(nameAt + 2).kind == TokenKind::RBracket) {
		for (size_t i = 0; i < nameAt + 3; ++i) {
			advance();
		}
		expect(TokenKind::Semicolon, "after a [[fallthrough]] attribute");
		if (_switchDepth == 0) {
			throw CompileError("fallthrough annotation is outside switch statement");
		}
		statement->kind = StatementKind::ExpressionStatement;
		statement->isFallthrough = true;
		return statement;
	}

	// A declaration statement starts with a type. Distinguish it from an
	// expression by looking for a bare type name, a type qualifier or a known
	// address space before anything that could begin an expression. A struct
	// declared earlier in the unit counts, since "Vertex vtx;" is a declaration
	// and nothing else could start with those two words.
	{
		Type probe;
		size_t nameTokens = 0;
		const std::string name = kind() == TokenKind::Identifier || at(TokenKind::ColonColon)
			? peekResolved(nameTokens) : std::string();
		// A local hides a struct tag of its own spelling only where it is not
		// qualified with "::": a bare name that could be the local or the tag is the
		// local, but "A::S" or "::S" still names the struct (j1).
		const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
			&& isLocal(name);
		// "S{1,2}.a = 3;" starts an expression: a declaration names a variable after the type.
		Type typedefOf;
		const bool bracedTemporary = kind() == TokenKind::Identifier && lookahead().kind == TokenKind::LBrace
			&& (_unit.findStruct(name) != nullptr
				|| (resolveTypeName(name, typedefOf) && _unit.findStruct(typedefOf.namedType) != nullptr));
		size_t arrayTokens = 0;
		const bool stdArrayType = (kind() == TokenKind::Identifier || at(TokenKind::ColonColon))
			&& !isLocal(name) && (atStdArray(arrayTokens) || atLibraryVec(arrayTokens) || atTraitType(arrayTokens));
		const bool looksLikeType = !bracedTemporary &&
			((kind() == TokenKind::Identifier || at(TokenKind::ColonColon)) && !objectHidesTag(name) && (resolveTypeName(name, probe)
				|| isTypeQualifier(name) || isResourceTypeName(name)
				|| (_unit.findStruct(name) != nullptr && !localHidesStruct)))
			|| stdArrayType
			|| (kind() == TokenKind::Identifier && (isIntegerSpecifier(current().text) || current().text == "auto"))
			|| atKeyword("enum") || atKeyword("device") || atKeyword("constant")
			|| atKeyword("threadgroup") || atKeyword("thread");

		if (looksLikeType) {
			statement->kind = StatementKind::DeclarationStatement;
			VariableDeclaration declaration;
			Type base;
			const size_t typeFrom = _position;
			declaration.type = parseType(true, true, &base);
			_localIndependent = typeTokensIndependent(typeFrom);
			_localInjected = namesInjectedClass(typeFrom);
			// An array alias ends in its name; written brackets end in "]".
			if (declaration.type.arrayLength && !declaration.type.isStdArray && _tokens[_position - 1].kind == TokenKind::RBracket) {
				throw CompileError("brackets are not allowed here; write the array length after the name, as float v[N]");
			}
			if (declaration.type.isAuto && declaration.type.isPointer) {
				throw CompileError("a local pointer declared with auto is not lowered yet");
			}
			declaration.isReference = at(TokenKind::Ampersand);
			rejectLocalQualifiers(declaration.type, declaration.isReference, true);
			if (declaration.isReference) {
				advance();
				if (declaration.type.isPointer || (declaration.type.arrayLength && !declaration.type.isStdArray)
					|| declaration.type.resource != ResourceKind::None) {
					throw CompileError("a reference to a pointer, an array or a resource is not lowered yet");
				}
			}

			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
			}
			declaration.name = std::string(advance().text);
			declareLocal(declaration.name);
			noteLocal(declaration.name);

			// "float a[4]" and "float a[] = {...}": the length follows the name.
			bool deduceLength = false;
			if (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
				if (declaration.isReference || declaration.type.isPointer || declaration.type.arrayLength
					|| declaration.type.resource != ResourceKind::None) {
					throw CompileError("an array of references, pointers or resources is not lowered yet");
				}
				if (declaration.type.isAuto) {
					throw CompileError("an array cannot be declared with auto; write the element type");
				}
				deduceLength = parseArrayDimensions(declaration.type);
				if (deduceLength && !at(TokenKind::Assign)) {
					throw CompileError("the array \"" + declaration.name + "\" has no size and needs an initialiser list");
				}
			}

			if (declaration.isReference) {
				if (!match(TokenKind::Assign)) {
					throw CompileError("the reference \"" + declaration.name + "\" needs an initialiser");
				}
				declaration.initializer = parseAssignment();
				recordObjectType(declaration.name, declaration.type);
				statement->declaration = std::move(declaration);
				expect(TokenKind::Semicolon, "after a reference declaration");
				return statement;
			}

			if (declaration.type.resource != ResourceKind::None) {
				parseSamplerLocal(declaration);
				statement->declaration = std::move(declaration);
				return statement;
			}

			parseLocalInitializer(declaration);
			if (deduceLength) {
				deduceArrayLength(declaration);
			}
			recordConstant(declaration, true);
			recordObjectType(declaration.name, declaration.type);

			// "T a, b = 1, c;" declares each name from the type before the first
			// declarator's stars and brackets, which bind to that name alone.
			while (at(TokenKind::Comma)) {
				if (declaration.type.isAuto) {
					throw CompileError("several declarators after auto are not lowered yet; Apple requires each to "
						"deduce the same type, so declare each variable on its own line");
				}
				advance();
				auto next = std::make_unique<Statement>();
				next->kind = StatementKind::DeclarationStatement;
				next->line = line();
				next->declaration = parseNextLocalDeclarator(base);
				_declaratorTail.push_back(std::move(next));
			}

			expect(TokenKind::Semicolon, "after a declaration");
			statement->declaration = std::move(declaration);
			return statement;
		}
	}

	statement->kind = StatementKind::ExpressionStatement;
	statement->expression = parseExpression();
	expect(TokenKind::Semicolon, "after an expression statement");

	return statement;
}

// A declarator after the first in "T a, *b, c[2] = {...};", after its comma.
VariableDeclaration Parser::parseNextLocalDeclarator(const Type& base) {
	VariableDeclaration another;
	if (at(TokenKind::Ampersand)) {
		throw CompileError("a reference after the first declarator is not lowered yet; "
			"references are not lowered in a declaration with several declarators");
	}
	another.type = base;
	parseDeclaratorStars(another.type);
	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
	}
	another.name = std::string(advance().text);
	declareLocal(another.name);
	noteLocal(another.name);

	bool deduceLength = false;
	if (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
		if (another.type.isPointer) {
			throw CompileError("an array of pointers is not lowered yet");
		}
		deduceLength = parseArrayDimensions(another.type);
	}
	parseLocalInitializer(another);
	if (deduceLength) {
		deduceArrayLength(another);
	}
	recordConstant(another, true);
	recordObjectType(another.name, another.type);
	return another;
}

// A sampler declared in the shader: "constexpr sampler s(options);", with a
// brace list or "= sampler(options)" for the options, or with none. Apple's
// compiler takes the same declaration without constexpr, so this does too.
void Parser::parseSamplerLocal(VariableDeclaration& declaration) {
	const Type& type = declaration.type;
	if (type.resource != ResourceKind::Sampler) {
		if ((type.isPointer && !isResourcePointer(type)) || type.arrayLength || (type.addressSpace != AddressSpace::None
			&& type.addressSpace != AddressSpace::Thread)) {
			throw CompileError("a local texture \"" + declaration.name + "\" that is a pointer, an array or in an "
				"address space other than thread is not lowered yet");
		}
		// A texture local is another name for a texture the function already names: sema binds it to
		// the same descriptor. Reassigning it, or choosing it at run time, has no such model.
		if (!match(TokenKind::Assign)) {
			throw CompileError("the texture \"" + declaration.name + "\" needs an initialiser that names a "
				"texture, as in \"texture2d<float> b = a;\": a texture local is another name for one, and "
				"mslc does not lower one that is bound later");
		}
		declaration.initializer = parseAssignment();
		expect(TokenKind::Semicolon, "after a declaration");
		return;
	}
	// "thread sampler* q = &s;": another name for a sampler the function already names, as a texture pointer is.
	if (isResourcePointer(type)) {
		if (!match(TokenKind::Assign)) {
			throw CompileError("the sampler pointer \"" + declaration.name + "\" needs an initialiser that is the "
				"address of a sampler: mslc does not lower one that is bound later");
		}
		declaration.initializer = parseAssignment();
		expect(TokenKind::Semicolon, "after a declaration");
		return;
	}
	if (type.isPointer || type.arrayLength
		|| (type.addressSpace != AddressSpace::None && type.addressSpace != AddressSpace::Thread
			&& type.addressSpace != AddressSpace::Constant)) {
		throw CompileError("a local sampler \"" + declaration.name + "\" with a pointer, an array "
			"or an address space other than thread is not lowered yet");
	}

	if (match(TokenKind::LParen)) {
		if (at(TokenKind::RParen)) {
			throw CompileError("\"" + declaration.name + "\" with empty parentheses declares a function, "
				"not a sampler; write the sampler with no parentheses or with braces");
		}
		declaration.sampler = parseSamplerOptions(TokenKind::RParen);
	} else if (match(TokenKind::LBrace)) {
		declaration.sampler = parseSamplerOptions(TokenKind::RBrace);
	} else if (match(TokenKind::Assign)) {
		if (match(TokenKind::LBrace)) {
			declaration.sampler = parseSamplerOptions(TokenKind::RBrace);
			expect(TokenKind::Semicolon, "after a declaration");
			return;
		}
		if (kind() != TokenKind::Identifier || current().text != "sampler"
			|| lookahead().kind != TokenKind::LParen) {
			// A copy of a sampler: sema binds the name to the same descriptor.
			declaration.initializer = parseAssignment();
			expect(TokenKind::Semicolon, "after a declaration");
			return;
		}
		advance();
		advance();
		declaration.sampler = parseSamplerOptions(TokenKind::RParen);
	} else {
		declaration.sampler = SamplerState();
	}

	expect(TokenKind::Semicolon, "after a declaration");
}

// The options of a constexpr sampler, "mag_filter::linear, address::repeat", up
// to and including the closing token. The restrictions are Apple's: pixel
// coordinates allow only equal filters, no mip filter and the clamp address modes.
SamplerState Parser::parseSamplerOptions(TokenKind closing) {
	SamplerState state;
	std::set<std::string> assigned;
	// An axis spelled clamp_to_zero (clamp_to_border is the same mode here): Apple refuses it beside a
	// border color other than transparent black, which indium would apply to it.
	bool zeroSpelled = false;

	while (!at(closing) && !at(TokenKind::EndOfFile)) {
		if (kind() == TokenKind::Identifier && current().text == "metal" && lookahead().kind == TokenKind::ColonColon) {
			advance();
			advance();
		} else if (const size_t prefix = libraryPrefixTokens(); prefix && lookahead(prefix).kind == TokenKind::Identifier) {
			for (size_t i = 0; i < prefix; ++i) {
				advance();
			}
		}
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected a sampler option, found " + std::string(tokenKindName(kind())));
		}

		const std::string family(advance().text);
		static const std::set<std::string> unlowered = {
			"reduction",
		};
		static const std::set<std::string> known = {
			"coord", "address", "s_address", "t_address", "r_address", "filter",
			"mag_filter", "min_filter", "mip_filter", "compare_func", "max_anisotropy", "lod_clamp", "border_color",
		};
		if (unlowered.count(family)) {
			throw CompileError("the sampler option \"" + family + "\" is accepted by Apple but not lowered: Vulkan "
				"selects it with VkSamplerReductionMode (VK_EXT_sampler_filter_minmax) and the reflection's sampler "
				"has no field for it, so it would be dropped silently; mslc lowers "
				"coord, address, s_address, t_address, r_address, filter, mag_filter, min_filter, "
				"mip_filter, compare_func, max_anisotropy, lod_clamp and border_color");
		}
		if (!known.count(family)) {
			throw CompileError("\"" + family + "\" is not a sampler option");
		}

		if (family == "max_anisotropy" || family == "lod_clamp") {
			// A call-like option: a number, or for lod_clamp the lower and upper bound.
			expect(TokenKind::LParen, ("after \"" + family + "\"").c_str());
			const auto number = [&]() {
				const bool negative = match(TokenKind::Minus);
				if (kind() != TokenKind::IntegerLiteral && kind() != TokenKind::FloatLiteral) {
					throw CompileError("\"" + family + "\" takes number literals");
				}
				const double value = kind() == TokenKind::IntegerLiteral ? static_cast<double>(current().integerValue)
					: current().floatValue;
				advance();
				if (!std::isfinite(value) || std::fabs(value) > std::numeric_limits<float>::max()) {
					throw CompileError("\"" + family + "\" takes a finite float");
				}
				return negative ? -value : value;
			};
			if (family == "max_anisotropy") {
				const double value = number();
				if (value < 1 || value > 16 || value != static_cast<uint32_t>(value)) {
					throw CompileError("max_anisotropy takes an integer from 1 to 16");
				}
				if (assigned.insert("anisotropy").second) {
					state.anisotropy = static_cast<uint32_t>(value);
				}
			} else {
				const double low = number();
				expect(TokenKind::Comma, "between the bounds of lod_clamp");
				const double high = number();
				if (low < 0 || high < low) {
					throw CompileError("lod_clamp takes a lower bound of at least 0 and an upper bound not below it");
				}
				if (assigned.insert("lod").second) {
					state.lodMin = static_cast<float>(low);
					state.lodMax = static_cast<float>(high);
				}
			}
			expect(TokenKind::RParen, ("to close \"" + family + "\"").c_str());
			if (!match(TokenKind::Comma)) {
				break;
			}
			if (at(closing)) {
				throw CompileError("expected a sampler option after ',' in a sampler's option list");
			}
			continue;
		}

		expect(TokenKind::ColonColon, "after a sampler option name");
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected a value after \"" + family + "::\"");
		}
		const std::string value(advance().text);
		const std::string spelled = family + "::" + value;

		const auto address = [&]() -> SamplerAddress {
			if (value == "clamp_to_zero" || value == "clamp_to_border") return SamplerAddress::ClampToZero;
			if (value == "clamp_to_edge") return SamplerAddress::ClampToEdge;
			if (value == "repeat") return SamplerAddress::Repeat;
			if (value == "mirrored_repeat") return SamplerAddress::MirroredRepeat;
			throw CompileError("\"" + spelled + "\" is not a sampler address mode");
		};
		const auto filter = [&]() -> SamplerFilter {
			if (value == "nearest") return SamplerFilter::Nearest;
			if (value == "linear") return SamplerFilter::Linear;
			if (value == "bicubic") {
				throw CompileError("\"" + spelled + "\" is accepted by Apple but not lowered: Vulkan has bicubic "
					"filtering only as VK_EXT_filter_cubic / VK_IMG_filter_cubic, outside core, and the reflection's "
					"sampler has no field for it, so it would be dropped silently");
			}
			throw CompileError("\"" + spelled + "\" is not a sampler filter");
		};

		// Apple keeps the first option that names an attribute and ignores a later
		// one for it: "address::repeat, address::clamp_to_edge" is repeat, and
		// "s_address::repeat, address::clamp_to_edge" repeats s and clamps t and r.
		// So an option only fills the attributes nothing before it set.
		const auto fill = [&](std::initializer_list<const char*> attributes, auto&& assign) {
			for (const char* attribute: attributes) {
				if (assigned.insert(attribute).second) {
					assign(attribute);
				}
			}
		};

		if (family == "coord") {
			if (value != "normalized" && value != "pixel") {
				throw CompileError("\"" + spelled + "\" is not a coordinate mode");
			}
			fill({ "coord" }, [&](const char*) { state.normalizedCoordinates = value != "pixel"; });
		} else if (family == "address" || family == "s_address" || family == "t_address"
			|| family == "r_address") {
			const SamplerAddress mode = address();
			fill(family == "address" ? std::initializer_list<const char*>{ "s", "t", "r" }
					: family == "s_address" ? std::initializer_list<const char*>{ "s" }
					: family == "t_address" ? std::initializer_list<const char*>{ "t" }
					: std::initializer_list<const char*>{ "r" },
				[&](const char* attribute) {
					(attribute[0] == 's' ? state.sAddress : attribute[0] == 't' ? state.tAddress
						: state.rAddress) = mode;
					zeroSpelled = zeroSpelled || value == "clamp_to_zero";
				});
		} else if (family == "filter" || family == "mag_filter" || family == "min_filter") {
			const SamplerFilter chosen = filter();
			fill(family == "filter" ? std::initializer_list<const char*>{ "mag", "min" }
					: family == "mag_filter" ? std::initializer_list<const char*>{ "mag" }
					: std::initializer_list<const char*>{ "min" },
				[&](const char* attribute) {
					(attribute[1] == 'a' ? state.magFilter : state.minFilter) = chosen;
				});
		} else if (family == "border_color") {
			static const std::map<std::string_view, SamplerBorder> borders = {
				{ "transparent_black", SamplerBorder::TransparentBlack },
				{ "opaque_black", SamplerBorder::OpaqueBlack }, { "opaque_white", SamplerBorder::OpaqueWhite },
			};
			const auto chosen = borders.find(value);
			if (chosen == borders.end()) {
				throw CompileError("\"" + spelled + "\" is not a border color");
			}
			fill({ "border" }, [&](const char*) { state.border = chosen->second; });
		} else if (family == "compare_func") {
			static const std::map<std::string_view, SamplerCompare> compares = {
				{ "never", SamplerCompare::Never }, { "less", SamplerCompare::Less },
				{ "less_equal", SamplerCompare::LessEqual }, { "greater", SamplerCompare::Greater },
				{ "greater_equal", SamplerCompare::GreaterEqual }, { "equal", SamplerCompare::Equal },
				{ "not_equal", SamplerCompare::NotEqual }, { "always", SamplerCompare::Always },
			};
			const auto chosen = compares.find(value);
			if (chosen == compares.end()) {
				throw CompileError("\"" + spelled + "\" is not a comparison function");
			}
			fill({ "compare" }, [&](const char*) { state.compare = chosen->second; });
		} else {
			SamplerMipFilter chosen = SamplerMipFilter::None;
			if (value == "none") chosen = SamplerMipFilter::None;
			else if (value == "nearest") chosen = SamplerMipFilter::Nearest;
			else if (value == "linear") chosen = SamplerMipFilter::Linear;
			else throw CompileError("\"" + spelled + "\" is not a mip filter");
			fill({ "mip" }, [&](const char*) { state.mipFilter = chosen; });
		}

		if (!match(TokenKind::Comma)) {
			break;
		}
		if (at(closing)) {
			throw CompileError("expected a sampler option after ',' in a sampler's option list");
		}
	}

	expect(closing, "to close a sampler's option list");

	if (zeroSpelled && state.border != SamplerBorder::TransparentBlack) {
		throw CompileError("invalid values combination in sampler initialization: clamp_to_zero takes no "
			"border_color other than transparent_black");
	}
	if (!state.normalizedCoordinates) {
		if (state.compare != SamplerCompare::Never) {
			throw CompileError("invalid values combination in sampler initialization: with coord::pixel the "
				"compare_func has to be never");
		}
		const auto clamps = [](SamplerAddress mode) {
			return mode == SamplerAddress::ClampToZero || mode == SamplerAddress::ClampToEdge;
		};
		if (assigned.count("anisotropy") || assigned.count("lod")) {
			throw CompileError("invalid values combination in sampler initialization: with coord::pixel, Vulkan "
				"needs anisotropy off and no lod range, so max_anisotropy and lod_clamp are refused");
		}
		if (state.magFilter != state.minFilter || state.mipFilter != SamplerMipFilter::None
			|| !clamps(state.sAddress) || !clamps(state.tAddress) || !clamps(state.rAddress)) {
			throw CompileError("invalid values combination in sampler initialization: with "
				"coord::pixel the min_filter and mag_filter have to be the same, the mip_filter "
				"none, and the address modes clamp_to_zero, clamp_to_edge or clamp_to_border");
		}
	}

	return state;
}

} // namespace mslc
