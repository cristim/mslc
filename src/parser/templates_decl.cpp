#include "parser/internal.h"

namespace mslc {

// The parameters of a template, from the "<" to the ">" that closes them.
void Parser::parseTemplateHeader(FunctionTemplate& declaration) {
	expect(TokenKind::Less, "to open a template parameter list");
	std::set<std::string> names;
	for (;;) {
		TemplateParameter parameter;
		if (atKeyword("template")) {
			throw CompileError("a template template parameter is not lowered");
		}
		if (at(TokenKind::Ellipsis)) {
			throw CompileError("a template parameter pack is not lowered");
		}
		if (atKeyword("typename") || atKeyword("class")) {
			advance();
			if (at(TokenKind::Ellipsis)) {
				throw CompileError("a template parameter pack is not lowered");
			}
		} else {
			// A non-type parameter: an integer or a bool. Any other type (a parameter whose type is
			// another parameter, an enum, a pointer) is a template mslc does not instantiate.
			parameter.isType = false;
			const Type type = parseType();
			if (!type.namedType.empty() || type.isVector() || type.isMatrix() || type.isPointer || type.arrayLength
				|| type.resource != ResourceKind::None || !type.enumName.empty() || type.isAtomic || type.isPacked
				|| type.scalar == ScalarKind::Void || type.scalar == ScalarKind::Half || type.scalar == ScalarKind::Float
				|| type.scalar == ScalarKind::Double) {
				throw CompileError("a non-type template parameter of that type is not lowered; mslc takes an "
					"integer or a bool");
			}
			parameter.kind = type.scalar;
			if (at(TokenKind::Ellipsis)) {
				throw CompileError("a template parameter pack is not lowered");
			}
		}
		if (kind() == TokenKind::Identifier) {
			parameter.name = std::string(advance().text);
			if (!names.insert(parameter.name).second) {
				throw CompileError("redeclaration of the template parameter \"" + parameter.name + "\"");
			}
		}
		if (match(TokenKind::Assign)) {
			// The default's tokens, up to the "," or ">" that ends it. A "<" inside it opens a nested
			// list; the same ">>" is two closers, and one that would close the header too is refused.
			parameter.hasDefault = true;
			const size_t first = _position;
			size_t angles = 0;
			for (;;) {
				if (at(TokenKind::EndOfFile)) {
					throw CompileError("expected '>' to close a template parameter list");
				}
				if (at(TokenKind::Less)) {
					++angles;
				} else if (at(TokenKind::Greater)) {
					if (angles == 0) {
						break;
					}
					--angles;
				} else if (at(TokenKind::ShiftRight)) {
					if (angles == 0) {
						throw CompileError("unbalanced '>>' in a template parameter list");
					}
					if (angles == 1) {
						// One '>' closes the default's own list and the other the header's: split the token.
						_tokens[_position].kind = TokenKind::Greater;
						_tokens[_position].text = ">";
						_tokens.insert(_tokens.begin() + _position, _tokens[_position]);
						continue;
					}
					angles -= 2;
				} else if (at(TokenKind::Comma) && angles == 0) {
					break;
				} else if (at(TokenKind::LParen)) {
					skipBalanced(TokenKind::LParen, TokenKind::RParen);
					continue;
				}
				advance();
			}
			parameter.defaultTokens.assign(_tokens.begin() + first, _tokens.begin() + _position);
			parameter.defaultTokens.emplace_back();
		}
		declaration.parameters.push_back(std::move(parameter));
		if (match(TokenKind::Comma)) {
			continue;
		}
		if (at(TokenKind::Greater)) {
			advance();
			return;
		}
		throw CompileError("expected '>' to close a template parameter list");
	}
}


Stage stageForKeyword(std::string_view word) {
	return word == "kernel" ? Stage::Kernel : word == "vertex" ? Stage::Vertex
		: word == "fragment" ? Stage::Fragment : Stage::None;
}


// The declarator of a function template, after its header, to the end of its body or ';': fills the
// tokens, the signature and whether it has a body, and returns the name. A member also takes "const" after
// its parameters.
std::string Parser::readTemplateFunction(FunctionTemplate& declaration, size_t headerStart, bool member,
	std::pair<size_t, size_t>* explicitArguments) {
	// The declarator, up to the parameter list. The name is the identifier right before
	// the first "(" outside angle brackets, and only a group that is followed by what a
	// function can have after its parameters is taken as the list. Attributes are
	// skipped whole, so their own parentheses are never mistaken for it. Any other shape
	// (decltype or typeof in the return type, a parenthesised name) cannot be told apart
	// from it by a rule, so it is refused: a wrong name would let a helper take the call.
	const size_t declaratorStart = _position;
	std::string name;
	size_t angles = 0;
	// An explicit specialization may name its arguments after the name, "f<int>(...)": the name before a
	// "<" at the outermost level, and that group when the parameter list follows it.
	std::string groupName;
	size_t groupStart = 0;
	size_t groupEnd = 0;
	for (;;) {
		if (at(TokenKind::EndOfFile) || at(TokenKind::Semicolon) || at(TokenKind::LBrace)
			|| at(TokenKind::Assign)) {
			throw CompileError("a template declaration that is not a function is not lowered");
		}
		if (at(TokenKind::LParen)) {
			if (angles == 0) {
				if (explicitArguments && !groupName.empty() && name.empty() && groupEnd + 1 == _position) {
					name = groupName;
					*explicitArguments = { groupStart, groupEnd };
				}
				break;
			}
			// A "(" inside the return type's own angle brackets, as in enable_if<(N > 2)>.
			skipBalanced(TokenKind::LParen, TokenKind::RParen);
			continue;
		}
		if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
			skipBalanced(TokenKind::LBracket, TokenKind::RBracket);
			continue;
		}
		if (atKeyword("__attribute__") && lookahead().kind == TokenKind::LParen) {
			advance();
			skipBalanced(TokenKind::LParen, TokenKind::RParen);
			continue;
		}
		if (atKeyword("operator")) {
			throw CompileError("an operator template is not lowered");
		}
		if (angles == 0 && !member && kind() == TokenKind::Identifier && declaration.stage == Stage::None
			&& stageForKeyword(current().text) != Stage::None) {
			throw CompileError("an attribute or specifier before \"" + std::string(current().text)
				+ "\" in an entry-point template is not lowered");
		}
		if (at(TokenKind::Less)) {
			if (angles == 0) {
				groupName = name;
				groupStart = _position;
			}
			++angles;
		} else if (at(TokenKind::Greater) && angles > 0) {
			--angles;
			groupEnd = angles == 0 ? _position : groupEnd;
		} else if (at(TokenKind::ShiftRight) && angles > 1) {
			angles -= 2;
			groupEnd = angles == 0 ? _position : groupEnd;
		}
		name = kind() == TokenKind::Identifier ? std::string(current().text) : std::string();
		advance();
	}
	if (name.empty()) {
		throw CompileError("a template declaration that is not a function is not lowered");
	}
	const auto notNameable = [&] {
		return CompileError("the declarator of the template \"" + name + "\" is not a shape mslc can name "
			"the function in (a decltype or typeof return type, or a parenthesised name); it is not lowered");
	};
	// Defence in depth: the follower and second-group checks below already refuse these.
	if (name == "decltype" || name == "__typeof__" || name == "typeof" || name == "sizeof"
		|| name == "alignof" || name == "noexcept") {
		throw notNameable();
	}
	if (declaration.constructorMember && (lookahead().kind == TokenKind::RParen
			|| (lookahead().kind == TokenKind::Identifier && lookahead().text == "void"
				&& lookahead(2).kind == TokenKind::RParen))) {
		throw CompileError("constructor template \"" + name + "\" takes no arguments, which is not lowered yet");
	}

	skipBalanced(TokenKind::LParen, TokenKind::RParen);
	if (!(at(TokenKind::EndOfFile) || at(TokenKind::LBrace) || at(TokenKind::Semicolon) || at(TokenKind::Assign)
		|| at(TokenKind::Arrow) || atKeyword("__attribute__") || (member && atKeyword("const"))
		|| (declaration.constructorMember && at(TokenKind::Colon)))) {
		throw notNameable();
	}
	// To the body or ';'. A real parameter list is never followed by a second group
	// before a trailing return type, so one means the first was a decltype or a name.
	if (declaration.constructorMember && at(TokenKind::Colon)) {
		advance();
		while (kind() == TokenKind::Identifier
			&& (lookahead().kind == TokenKind::LParen || lookahead().kind == TokenKind::LBrace)) {
			advance();
			if (at(TokenKind::LParen)) {
				skipBalanced(TokenKind::LParen, TokenKind::RParen);
			} else {
				skipBalanced(TokenKind::LBrace, TokenKind::RBrace);
			}
			if (!match(TokenKind::Comma)) {
				break;
			}
		}
	}
	while (!at(TokenKind::LBrace) && !at(TokenKind::Semicolon)) {
		if (at(TokenKind::EndOfFile)) {
			throw CompileError("expected a body or ';' after the template function \"" + name + "\"");
		}
		if (at(TokenKind::Arrow)) {
			while (!at(TokenKind::LBrace) && !at(TokenKind::Semicolon) && !at(TokenKind::EndOfFile)) {
				advance();
			}
			continue;
		}
		if (atKeyword("__attribute__") && lookahead().kind == TokenKind::LParen) {
			advance();
			skipBalanced(TokenKind::LParen, TokenKind::RParen);
		} else if (at(TokenKind::LParen)) {
			throw notNameable();
		} else {
			advance();
		}
	}
	const size_t signatureEnd = _position;
	declaration.hasBody = at(TokenKind::LBrace);
	if (declaration.hasBody) {
		skipBalanced(TokenKind::LBrace, TokenKind::RBrace);
	} else {
		advance();
	}

	for (size_t i = headerStart; i < signatureEnd; ++i) {
		declaration.signature += std::string(_tokens[i].text) + " ";
	}
	declaration.tokens.assign(_tokens.begin() + declaratorStart, _tokens.begin() + _position);
	declaration.tokens.emplace_back();
	return name;
}

// "template RET name<arguments>(parameters);" of a helper function template, or
// "template [[host_name("...")]] stage RET name<arguments>(parameters);" of an entry point.
// Entry-point instantiations are queued and lowered into _unit.functions at the end of parse().
void Parser::parseExplicitInstantiation() {
	const size_t startLine = line();
	std::optional<std::string> hostName;
	while (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
		advance(); advance();
		if (!at(TokenKind::RBracket)) {
			do {
				if (kind() != TokenKind::Identifier) { throw CompileError("expected an attribute name in an explicit template instantiation"); }
				const std::string attr(advance().text);
				if (attr != "host_name") { throw CompileError("attribute \"" + attr + "\" on an explicit template instantiation is not lowered"); }
				if (hostName) { throw CompileError("attribute \"host_name\" cannot appear more than once"); }
				expect(TokenKind::LParen, "after [[host_name");
				if (!at(TokenKind::StringLiteral)) { throw CompileError("[[host_name]] needs a string literal argument"); }
				std::string value;
				while (at(TokenKind::StringLiteral)) {
					const std::string_view raw = advance().text, inner = raw.substr(1, raw.size() - 2);
					if (inner.find('\\') != std::string_view::npos) { throw CompileError("escape sequences in [[host_name]] are not lowered"); }
					value.append(inner);
				}
				expect(TokenKind::RParen, "to close [[host_name]]");
				if (value.empty()) { throw CompileError("[[host_name]] cannot be empty"); }
				hostName = std::move(value);
			} while (match(TokenKind::Comma));
		}
		expect(TokenKind::RBracket, "to close an attribute list"); expect(TokenKind::RBracket, "to close an attribute list");
	}
	const Stage stage = kind() == TokenKind::Identifier ? stageForKeyword(current().text) : Stage::None;
	if (stage != Stage::None) {
		advance();
		if (!hostName) {
			throw CompileError(at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket
				? "an attribute list cannot appear after the stage keyword; write [[host_name(\"...\")]] before it"
				: "an explicit template instantiation of an entry point needs [[host_name(\"...\")]]");
		}
		if (atKeyword("inline") || atKeyword("static") || atKeyword("constexpr") || at(TokenKind::LBracket) || atKeyword("auto")) {
			throw CompileError("invalid specifier or attribute in an explicit template instantiation");
		}
		const Type returnType = parseType();
		QualifiedName spelled;
		if (!peekQualifiedName(spelled)) { throw CompileError("expected a function template name in an explicit template instantiation"); }
		for (size_t i = 0; i < spelled.tokens; ++i) { advance(); }
		const std::string spelledText = (spelled.global ? "::" : "") + joinParts(spelled.parts, 0, spelled.parts.size());
		std::vector<std::string> alternatives;
		std::set<std::string> candidateNames { resolveName(spelled, &alternatives) };
		candidateNames.insert(alternatives.begin(), alternatives.end());
		if (std::none_of(candidateNames.begin(), candidateNames.end(),
			[&](const std::string& each) { return _entryTemplateNames.count(each) != 0; })) {
			throw CompileError("an explicit instantiation of \"" + spelledText + "\" does not name an entry-point template");
		}
		std::vector<TemplateArgument> explicitArgs;
		if (match(TokenKind::Less)) {
			if (!at(TokenKind::Greater) && !at(TokenKind::ShiftRight)) {
				do { explicitArgs.push_back(parseTemplateArgument()); } while (match(TokenKind::Comma));
			}
			closeAngle(("to close explicit template arguments of \"" + spelledText + "\"").c_str());
		}
		expect(TokenKind::LParen, "to open the parameters of an explicit template instantiation");
		std::vector<Parameter> instParams;
		{
			const LocalScope parameters(*this);
			parseEntryParameterList(instParams, "explicit instantiation \"" + spelledText + "\"", true);
		}
		if (at(TokenKind::LBrace) || at(TokenKind::LBracket) || at(TokenKind::Arrow)) {
			throw CompileError(at(TokenKind::LBrace) ? "an explicit template instantiation cannot have a definition"
				: "an attribute or trailing return type after an explicit template instantiation is not lowered");
		}
		expect(TokenKind::Semicolon, "after an explicit template instantiation");
		_pendingEntryInstantiations.push_back({ _unit.functions.size(), startLine, stage, *hostName,
			spelledText, std::move(candidateNames), returnType, std::move(explicitArgs), std::move(instParams) });
		_unit.functions.push_back({ stage, {}, std::move(*hostName), {}, {}, startLine });
		return;
	}
	if (hostName) { throw CompileError("[[host_name]] is only supported on an explicit instantiation of an entry point"); }

	size_t open = 0;
	for (; lookahead(open).kind != TokenKind::LParen; ++open) {
		const Token& token = lookahead(open);
		if (token.kind == TokenKind::EndOfFile || token.kind == TokenKind::Semicolon) {
			throw CompileError("an explicit template instantiation is not lowered");
		}
		if (token.kind == TokenKind::LBracket || (token.kind == TokenKind::Identifier
			&& (token.text == "kernel" || token.text == "vertex" || token.text == "fragment"))) {
			throw CompileError("an explicit template instantiation of an entry point is not lowered");
		}
	}
	size_t name = open;
	if (name > 0 && lookahead(name - 1).kind == TokenKind::Greater) {
		int depth = 0;
		do {
			--name;
			depth += lookahead(name).kind == TokenKind::Greater ? 1 : lookahead(name).kind == TokenKind::Less ? -1 : 0;
		} while (depth > 0 && name > 0);
	}
	if (name == 0 || lookahead(name - 1).kind != TokenKind::Identifier
		|| (name > 1 && lookahead(name - 2).kind == TokenKind::ColonColon)) {
		throw CompileError("an explicit template instantiation is not lowered");
	}
	const std::string spelled(lookahead(name - 1).text);
	if (!_unit.functionTemplates.count(qualify(spelled)) && !_unit.functionTemplates.count(spelled)) {
		if (_entryTemplateNames.count(qualify(spelled)) || _entryTemplateNames.count(spelled)) {
			throw CompileError("an explicit template instantiation of entry-point template \"" + spelled
				+ "\" needs its stage keyword and [[host_name(\"...\")]]");
		}
		throw CompileError("an explicit instantiation of \"" + spelled + "\" does not name a function template");
	}
	while (!at(TokenKind::Semicolon)) {
		if (at(TokenKind::EndOfFile)) {
			throw CompileError("an explicit template instantiation is not terminated");
		}
		advance();
	}
	advance();
}

// "template" at file scope: a function template. It is parsed far enough to know its parameters,
// its name and where it ends; the tokens and the name state are kept, and a call that selects it
// has it re-parsed with its arguments bound (instantiateTemplate). A body is only meaningful with
// arguments (a dependent name has no type until then), so one that is never instantiated needs
// nothing more, as in clang.
void Parser::parseTemplateDeclaration() {
	advance();
	if (!at(TokenKind::Less)) {
		parseExplicitInstantiation();
		return;
	}
	if (lookahead().kind == TokenKind::Greater) {
		parseExplicitSpecialization(_position);
		return;
	}

	FunctionTemplate declaration;
	declaration.line = line();
	const size_t headerStart = _position;
	parseTemplateHeader(declaration);

	if (atKeyword("struct") || atKeyword("class") || atKeyword("union")) {
		ClassTemplate classDeclaration;
		classDeclaration.line = declaration.line;
		classDeclaration.parameters = std::move(declaration.parameters);
		parseClassTemplate(classDeclaration);
		return;
	}
	if (atKeyword("using") || atKeyword("template")) {
		throw CompileError("an alias template is not lowered");
	}

	declaration.stage = kind() == TokenKind::Identifier ? stageForKeyword(current().text) : Stage::None;
	if (declaration.stage != Stage::None
		&& std::find(_namespacePath.begin(), _namespacePath.end(), kAnonymousNamespace) != _namespacePath.end()) {
		throw CompileError(std::string(current().text) + " function cannot be declared in anonymous namespace");
	}

	const std::string name = readTemplateFunction(declaration, headerStart, false);

	const std::string qualified = qualify(name);
	declaration.name = qualified;
	declaration.order = declaration.bodyOrder = _functionOrder++;
	declaration.globalsBefore = _unit.globals.size();
	if (const auto kinds = kindsOf(qualified); !kinds.empty() && std::any_of(kinds.begin(), kinds.end(),
		[](std::string_view kindName) { return kindName != "function"; })) {
		throw CompileError("the function template \"" + qualified + "\" shares its name with a type or a "
			"variable, which is not lowered: a call spelled like a construction would not reach it");
	}
	declareName(qualified, "function");
	if (declaration.stage != Stage::None) { rejectUsingConflict(qualified, "function", nullptr); }
	(declaration.stage == Stage::None ? _unit.functionTemplates : _entryTemplateNames).insert(qualified);
	std::vector<FunctionTemplate>& targetList = declaration.stage == Stage::None
		? _unit.templates : _unit.entryTemplates;
	declaration.endAt = _position;

	// A prototype and its definition are one template, found at the prototype's place in the file.
	for (FunctionTemplate& earlier: targetList) {
		if (earlier.name != qualified || earlier.signature != declaration.signature) {
			continue;
		}
		if (earlier.hasBody && declaration.hasBody) {
			throw CompileError("redefinition of the function template \"" + qualified + "\"");
		}
		if (declaration.hasBody) {
			declaration.order = earlier.order;
			earlier = std::move(declaration);
		}
		return;
	}
	targetList.push_back(std::move(declaration));
}

// "template <>" and a function: an explicit specialization of a function template declared above. It is
// kept as a template of no parameters, the explicit arguments taken out of its declarator, for sema to
// match against the primary templates of its name.
void Parser::parseExplicitSpecialization(size_t headerStart) {
	advance();
	advance();
	if (atKeyword("struct") || atKeyword("class") || atKeyword("union")) {
		throw CompileError("an explicit specialization of a class template is not lowered; Apple accepts it, and "
			"mslc refuses it so that the primary template is never silently used in its place");
	}
	if (atKeyword("template")) {
		throw CompileError("an explicit specialization of a member template is not lowered");
	}
	ExplicitSpecialization specialization;
	FunctionTemplate& declaration = specialization.declaration;
	declaration.line = line();
	std::pair<size_t, size_t> group { 0, 0 };
	const size_t declaratorStart = _position;
	const std::string name = readTemplateFunction(declaration, headerStart, false, &group);
	const std::string qualified = qualify(name);
	if (!_unit.functionTemplates.count(qualified)) {
		throw CompileError("\"" + qualified + "\" is explicitly specialized, and no function template of that name "
			"is declared above it");
	}
	if (!declaration.hasBody) {
		throw CompileError("the explicit specialization of \"" + qualified + "\" is declared and not defined here; "
			"a declaration without a body is not lowered");
	}
	const size_t end = _position;
	if (group.second) {
		_position = group.first + 1;
		const size_t before = _tokens.size();
		while (!at(TokenKind::Greater) && !at(TokenKind::ShiftRight)) {
			specialization.given.push_back(parseTemplateArgument());
			if (!match(TokenKind::Comma)) {
				break;
			}
		}
		// closeAngle splits a ">>" that closes the last argument and this list into two tokens.
		const size_t inserted = _tokens.size() - before;
		if (_position != group.second + inserted || (!at(TokenKind::Greater) && !at(TokenKind::ShiftRight))) {
			throw CompileError("the template arguments of the explicit specialization of \"" + qualified
				+ "\" are not lowered: expected \">\" where \"" + std::string(current().text) + "\" is");
		}
		_position = end + inserted;
		// The tokens without the arguments, so the declarator reads as a function's.
		const size_t from = group.first - declaratorStart;
		const size_t to = group.second + 1 - declaratorStart;
		declaration.tokens.erase(declaration.tokens.begin() + from, declaration.tokens.begin() + to);
	}
	declaration.name = qualified;
	declaration.order = declaration.bodyOrder = _functionOrder++;
	declaration.globalsBefore = _unit.globals.size();
	declaration.endAt = _position;
	_unit.specializations.push_back(std::move(specialization));
}

// Binds the template's parameters in the innermost block: a type is a typedef, a constant an
// enumerator-like value, each under a key no spelling in the source can reach, and the
// parameter's own name is mapped to it as a using declaration is, so a local of that name in a
// nested block still hides it.
void Parser::bindTemplateArguments(const FunctionTemplate& declaration, const std::vector<TemplateArgument>& arguments) {
	bindParameters(declaration.name, declaration.parameters, arguments);
}

void Parser::bindParameters(const std::string& owner, const std::vector<TemplateParameter>& parameters,
	const std::vector<TemplateArgument>& arguments) {
	if (arguments.size() != parameters.size()) {
		throw CompileError("the template \"" + owner + "\" takes " + std::to_string(parameters.size())
			+ " arguments, and " + std::to_string(arguments.size()) + " are bound");
	}
	for (size_t i = 0; i < arguments.size(); ++i) {
		const TemplateParameter& parameter = parameters[i];
		const TemplateArgument& argument = arguments[i];
		if (argument.isType != parameter.isType) {
			throw CompileError("the template argument for \"" + parameter.name + "\" of \"" + owner
				+ "\" has to be " + (parameter.isType ? "a type" : "a constant"));
		}
		if (parameter.name.empty()) {
			continue;
		}
		const std::string key = "<template " + parameter.name + ">";
		if (parameter.isType) {
			_typedefs[key] = argument.type;
		} else if (!argument.unbound) {
			_enumConstants[key] = EnumConstant { argument.value, parameter.kind, std::string() };
		}
		_blocks.back().names[parameter.name] = key;
	}
}

} // namespace mslc
