#include "preprocessor/internal.h"

namespace mslc {

namespace pp {

std::optional<Builtin> builtinByName(std::string_view name) {
	if (name == "__LINE__") return Builtin::Line;
	if (name == "__FILE__") return Builtin::File;
	if (name == "__BASE_FILE__") return Builtin::BaseFile;
	if (name == "__INCLUDE_LEVEL__") return Builtin::IncludeLevel;
	if (name == "__COUNTER__") return Builtin::Counter;
	return std::nullopt;
}

// Builtins of Apple's compiler that mslc does not provide. They are named
// rather than left to read as undefined, which in #if would be 0.
bool isUnsupportedBuiltin(std::string_view name) {
	static const char* const names[] = {
		"__DATE__", "__TIME__", "__TIMESTAMP__", "__FILE_NAME__", "__has_include_next",
		"__has_feature", "__has_extension", "__has_builtin", "__has_constexpr_builtin",
		"__has_attribute", "__has_cpp_attribute", "__has_c_attribute",
		"__has_declspec_attribute", "__has_warning", "__has_embed", "__is_identifier",
		"__is_target_arch", "__is_target_vendor", "__is_target_os",
		"__is_target_environment", "__is_target_variant_os",
		"__is_target_variant_environment", "__building_module", "__VA_OPT__",
	};

	for (const char* candidate: names) {
		if (name == candidate) {
			return true;
		}
	}
	return false;
}

// Names a #define or #undef may not touch, because they are computed or are
// operators rather than stored text.
bool isBuiltinName(std::string_view name) {
	return name == "defined" || name == "__VA_ARGS__" || name == "__has_include" || name == "_Pragma"
		|| builtinByName(name) || isUnsupportedBuiltin(name);
}

// C++'s spellings of operators, which Apple's compiler refuses as macro names.
bool isAlternativeOperator(std::string_view name) {
	static const char* const names[] = {
		"and", "or", "not", "xor", "bitand", "bitor", "compl", "and_eq", "or_eq", "xor_eq", "not_eq",
	};

	for (const char* candidate: names) {
		if (name == candidate) {
			return true;
		}
	}
	return false;
}

bool isKeyword(std::string_view name) {
	static const char* const keywords[] = {
		"alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch", "char",
		"class", "const", "const_cast", "constexpr", "continue", "decltype", "default",
		"delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export",
		"extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
		"mutable", "namespace", "new", "noexcept", "nullptr", "operator", "private",
		"protected", "public", "register", "reinterpret_cast", "return", "short", "signed",
		"sizeof", "static", "static_assert", "static_cast", "struct", "switch", "template",
		"this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
		"union", "unsigned", "using", "virtual", "void", "volatile", "while",
		// MSL's own qualifiers and entry-point keywords
		"constant", "device", "fragment", "kernel", "thread", "threadgroup", "vertex",
	};

	for (const char* candidate: keywords) {
		if (name == candidate) {
			return true;
		}
	}
	return false;
}

Directive directiveByName(std::string_view name) {
	static const std::map<std::string, Directive, std::less<>> table = {
		{ "define", Directive::Define }, { "undef", Directive::Undef },
		{ "include", Directive::Include }, { "if", Directive::If },
		{ "ifdef", Directive::Ifdef }, { "ifndef", Directive::Ifndef },
		{ "elif", Directive::Elif }, { "else", Directive::Else },
		{ "endif", Directive::Endif }, { "line", Directive::Line },
		{ "error", Directive::Error }, { "warning", Directive::Warning },
		{ "pragma", Directive::Pragma },
		{ "import", Directive::Import }, { "include_next", Directive::Unsupported },
		{ "ident", Directive::Unsupported }, { "sccs", Directive::Unsupported },
		{ "assert", Directive::Unsupported }, { "unassert", Directive::Unsupported },
	};

	const auto it = table.find(name);
	return it == table.end() ? Directive::Unknown : it->second;
}

} // namespace pp

using namespace pp;

void Preprocessor::requireNoExtra(const std::vector<Token>& line, size_t used, const char* directive,
	const Cursor& cursor) const {

	if (line.size() > used) {
		fail(locate(cursor, line[used]), std::string("extra tokens at the end of #") + directive
			+ " (" + spell(line, used) + ")");
	}
}

void Preprocessor::directive(Cursor& cursor) {
	const SourceFile& file = cursor.file;
	const Token& hash = file.tokens[cursor.index];
	const Loc hashLoc = locate(cursor, hash);

	size_t end = cursor.index + 1;
	while (file.tokens[end].kind != TokenKind::EndOfFile && !file.tokens[end].startOfLine) {
		++end;
	}
	const std::vector<Token> line(file.tokens.begin() + cursor.index + 1, file.tokens.begin() + end);
	cursor.index = end;

	const bool skipping = !cursor.conditionals.empty() && !cursor.conditionals.back().live;

	// The null directive: a '#' alone on its line.
	if (line.empty()) {
		return;
	}

	if (!isName(line[0])) {
		if (skipping) {
			return;
		}
		fail(locate(cursor, line[0]), "expected a preprocessor directive after '#'");
	}

	const Directive kind = directiveByName(line[0].text);
	const Loc nameLoc = locate(cursor, line[0]);

	switch (kind) {
		case Directive::If:
		case Directive::Ifdef:
		case Directive::Ifndef: {
			if (cursor.conditionals.size() >= kMaxConditionalDepth) {
				fail(hashLoc, "conditionals nested more than " + std::to_string(kMaxConditionalDepth) + " deep");
			}

			Conditional conditional;
			conditional.opener = hashLoc;

			if (skipping) {
				conditional.live = false;
				conditional.taken = true;
			} else if (kind == Directive::If) {
				conditional.live = evaluateCondition(cursor, line, nameLoc);
				conditional.taken = conditional.live;
			} else {
				const char* directiveName = kind == Directive::Ifdef ? "ifdef" : "ifndef";
				if (line.size() < 2 || !isName(line[1])) {
					fail(nameLoc, std::string("#") + directiveName + " expects a macro name");
				}
				requireNoExtra(line, 2, directiveName, cursor);
				const bool defined = isDefinedName(line[1].text, locate(cursor, line[1]));
				conditional.live = kind == Directive::Ifdef ? defined : !defined;
				conditional.taken = conditional.live;
			}

			cursor.conditionals.push_back(conditional);
			return;
		}

		case Directive::Elif: {
			if (cursor.conditionals.empty()) {
				fail(nameLoc, "#elif without #if");
			}
			Conditional& top = cursor.conditionals.back();
			if (top.seenElse) {
				fail(nameLoc, "#elif after #else");
			}

			// Not evaluated unless it can be taken (a group inside a skipped one
			// starts out taken), so an expression only skipped text could make
			// sense of is not diagnosed.
			if (top.taken) {
				top.live = false;
			} else {
				top.live = evaluateCondition(cursor, line, nameLoc);
				top.taken = top.live;
			}
			return;
		}

		case Directive::Else: {
			if (cursor.conditionals.empty()) {
				fail(nameLoc, "#else without #if");
			}
			Conditional& top = cursor.conditionals.back();
			if (top.seenElse) {
				fail(nameLoc, "#else after #else");
			}
			top.seenElse = true;
			top.live = !top.taken;
			top.taken = true;
			return;
		}

		case Directive::Endif: {
			if (cursor.conditionals.empty()) {
				fail(nameLoc, "#endif without #if");
			}
			cursor.conditionals.pop_back();
			return;
		}

		default:
			break;
	}

	if (skipping) {
		return;
	}

	switch (kind) {
		case Directive::Define: defineMacro(cursor, line, nameLoc); return;
		case Directive::Undef: undefMacro(cursor, line, nameLoc); return;
		case Directive::Include: includeDirective(cursor, line, nameLoc, false); return;
		case Directive::Import: includeDirective(cursor, line, nameLoc, true); return;
		case Directive::Line: lineDirective(cursor, line, nameLoc); return;
		case Directive::Pragma: pragmaDirective(cursor, line, nameLoc); return;

		case Directive::Error:
			fail(nameLoc, line.size() > 1 ? "#error directive in this source: " + spell(line, 1)
				: std::string("#error directive in this source"));

		case Directive::Warning:
			// mslc has no channel for warnings, and Apple's compiler continues past
			// one, so the directive is accepted and has no effect.
			return;

		case Directive::Unsupported:
			fail(nameLoc, "unsupported preprocessor directive \"#" + std::string(line[0].text)
				+ "\"; mslc handles #include, #import, #define, #undef, #if, #ifdef, #ifndef, #elif, #else, "
				"#endif, #line, #error, #warning and #pragma");

		default:
			break;
	}

	fail(nameLoc, "unknown preprocessor directive \"#" + std::string(line[0].text)
		+ "\"; mslc handles #include, #import, #define, #undef, #if, #ifdef, #ifndef, #elif, #else, #endif, "
		"#line, #error, #warning and #pragma");
}

void Preprocessor::pragmaDirective(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	if (line.size() >= 2 && isName(line[1])) {
		applyPragma(&cursor, line[1].text, at);
	}
}

// What #pragma and _Pragma share: the pragmas whose effect mslc honours, by first word.
void Preprocessor::applyPragma(Cursor* cursor, std::string_view first, const Loc& at) {
	if (first == "once") {
		if (cursor) {
			cursor->file.once = true;
		}
		return;
	}

	// These change which macros exist, so ignoring them would make a later
	// expansion differ from the reference compiler's.
	if (first == "push_macro" || first == "pop_macro") {
		fail(at, "#pragma " + std::string(first) + " is not supported");
	}

	// A pragma is advice to the compiler. Apple's accepts one it does not know,
	// and so does mslc, but a pragma that mslc does not act on (pack, unroll,
	// clang diagnostic) is dropped, not honoured.
}

void Preprocessor::lineDirective(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	std::deque<PPTok> work;
	for (size_t i = 1; i < line.size(); ++i) {
		work.push_back(fromFile(cursor, line[i]));
	}
	std::vector<PPTok> expanded;
	scan(nullptr, work, expanded, 1);

	if (expanded.empty()) {
		fail(at, "#line directive requires a positive integer argument");
	}

	const Token& number = expanded[0].tok;
	if (number.kind != TokenKind::IntegerLiteral
		|| number.text.find_first_not_of("0123456789") != std::string_view::npos
		|| number.text.size() > 10) {
		fail(at, "#line directive requires a simple digit sequence");
	}
	const unsigned long long value = std::stoull(std::string(number.text));
	if (value > 2147483647ULL) {
		fail(at, "#line number is out of range");
	}

	size_t used = 1;
	if (expanded.size() >= 2) {
		if (!isStringLiteral(expanded[1].tok)) {
			fail(at, "invalid filename for #line directive");
		}
		const std::string_view quoted = expanded[1].tok.text;
		cursor.nameId = internName(std::string(quoted.substr(1, quoted.size() - 2)));
		used = 2;
	}
	if (expanded.size() > used) {
		fail(at, "extra tokens at the end of #line");
	}

	// The line after the directive is the one that is numbered.
	const Loc lastLoc = locate(cursor, line.back());
	cursor.lineDelta = static_cast<long>(value) - static_cast<long>(lastLoc.line + 1);
}

void Preprocessor::undefMacro(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	if (line.size() < 2 || !isName(line[1])) {
		fail(at, "macro name missing in #undef");
	}
	requireNoExtra(line, 2, "undef", cursor);

	const std::string_view name = line[1].text;
	if (isAlternativeOperator(name)) {
		fail(locate(cursor, line[1]), "C++ operator '" + std::string(name) + "' used as a macro name");
	}
	if (isBuiltinName(name)) {
		fail(locate(cursor, line[1]), "cannot #undef \"" + std::string(name) + "\": it is a builtin");
	}

	const auto it = _macros.find(name);
	if (it != _macros.end()) {
		_macros.erase(it);
	}
}

void Preprocessor::defineMacro(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	if (line.size() < 2 || !isName(line[1])) {
		fail(at, "macro name missing in #define");
	}

	const Token& nameToken = line[1];
	const Loc nameLoc = locate(cursor, nameToken);
	const std::string name(nameToken.text);

	if (isBuiltinName(name)) {
		fail(nameLoc, "cannot #define \"" + name + "\": it is a builtin");
	}
	if (isAlternativeOperator(name)) {
		fail(nameLoc, "C++ operator '" + name + "' used as a macro name");
	}
	if (isKeyword(name)) {
		fail(nameLoc, "cannot #define \"" + name + "\": it is a keyword, and redefining it would change "
			"what the rest of the source means");
	}

	auto macro = std::make_unique<Macro>();
	macro->name = name;
	macro->definedAt = nameLoc;

	std::map<std::string_view, size_t> parameterIndex;

	size_t bodyStart = 2;
	if (line.size() > 2 && line[2].kind == TokenKind::LParen && !line[2].spaceBefore) {
		macro->functionLike = true;
		size_t i = 3;
		bool closed = false;

		while (i < line.size()) {
			if (line[i].kind == TokenKind::RParen && macro->params.empty() && !macro->variadic) {
				closed = true;
				++i;
				break;
			}

			// "..." names the variable arguments __VA_ARGS__; "args..." names them args.
			const bool anonymousVariadic = line[i].kind == TokenKind::Ellipsis;
			if (!anonymousVariadic && !isName(line[i])) {
				fail(locate(cursor, line[i]), "expected a macro parameter name, found \""
					+ std::string(line[i].text) + "\"");
			}
			if (!anonymousVariadic && line[i].text == "__VA_ARGS__") {
				fail(locate(cursor, line[i]), "__VA_ARGS__ cannot be a macro parameter name");
			}
			if (!anonymousVariadic && !parameterIndex.emplace(line[i].text, macro->params.size()).second) {
				fail(locate(cursor, line[i]), "duplicate macro parameter \"" + std::string(line[i].text) + "\"");
			}

			if (anonymousVariadic) {
				macro->variadic = true;
				++i;
			} else if (i + 1 < line.size() && line[i + 1].kind == TokenKind::Ellipsis) {
				macro->variadic = true;
				macro->variadicName = std::string(line[i].text);
				parameterIndex.erase(line[i].text);
				i += 2;
			} else {
				macro->params.emplace_back(line[i].text);
				++i;
			}

			if (macro->variadic) {
				if (i >= line.size() || line[i].kind != TokenKind::RParen) {
					fail(locate(cursor, line[std::min(i, line.size() - 1)]),
						"expected ) to close a macro parameter list after the variable arguments");
				}
				closed = true;
				++i;
				break;
			}

			if (i < line.size() && line[i].kind == TokenKind::RParen) {
				closed = true;
				++i;
				break;
			}
			if (i < line.size() && line[i].kind == TokenKind::Comma) {
				++i;
				if (i < line.size() && line[i].kind == TokenKind::RParen) {
					fail(locate(cursor, line[i]), "expected a macro parameter name after ','");
				}
				continue;
			}
			break;
		}

		if (!closed) {
			fail(nameLoc, "expected ) to close a macro parameter list");
		}
		bodyStart = i;
	}

	std::vector<Token> body(line.begin() + bodyStart, line.end());
	for (const Token& token: body) {
		if (token.kind == TokenKind::Invalid) {
			fail(locate(cursor, token), describeInvalidToken(token));
		}
	}

	const size_t named = macro->params.size();
	auto paramIndex = [&](const Token& token) -> std::optional<size_t> {
		if (!macro->functionLike || !isName(token)) {
			return std::nullopt;
		}
		const auto found = parameterIndex.find(token.text);
		if (found != parameterIndex.end()) {
			return found->second;
		}
		if (macro->variadic && token.text == macro->variadicName) {
			return named;
		}
		return std::nullopt;
	};

	for (size_t i = 0; i < body.size(); ++i) {
		const Token& token = body[i];
		BodyItem item;
		item.tok = token;
		item.spaceBefore = i > 0 && token.spaceBefore;

		if (token.text == "__VA_ARGS__" && !(macro->variadic && macro->variadicName == "__VA_ARGS__")) {
			fail(locate(cursor, token), "__VA_ARGS__ can only appear in the expansion of a variadic macro");
		}
		if (token.kind == TokenKind::HashHash) {
			if (i == 0 || i + 1 == body.size()) {
				fail(locate(cursor, token), "'##' cannot appear at either end of a macro expansion");
			}
			if (body[i + 1].kind == TokenKind::HashHash) {
				fail(locate(cursor, token), "pasting formed '##', which is not a valid preprocessing token");
			}
			item.kind = BodyKind::Paste;
		} else if (macro->functionLike && token.kind == TokenKind::Hash) {
			if (i + 1 >= body.size() || !paramIndex(body[i + 1])) {
				fail(locate(cursor, token), "'#' is not followed by a macro parameter");
			}
			item.kind = BodyKind::Stringify;
			item.param = *paramIndex(body[i + 1]);
			++i;
		} else if (const auto index = paramIndex(token)) {
			item.kind = BodyKind::Param;
			item.param = *index;
		}

		macro->body.push_back(std::move(item));
	}

	const auto existing = _macros.find(name);
	if (existing != _macros.end()) {
		const Macro& old = *existing->second;
		bool same = old.functionLike == macro->functionLike && old.variadic == macro->variadic
			&& old.params == macro->params && old.variadicName == macro->variadicName
			&& old.body.size() == macro->body.size();
		for (size_t i = 0; same && i < old.body.size(); ++i) {
			const BodyItem& a = old.body[i];
			const BodyItem& b = macro->body[i];
			same = a.kind == b.kind && a.param == b.param && a.spaceBefore == b.spaceBefore
				&& (a.kind != BodyKind::Token || a.tok.text == b.tok.text);
		}
		if (!same) {
			// QUIRK: a #define in the source of a name given through the options replaces the option's
			// value instead of conflicting with it. The compiler of the Metal toolchain accepts that with
			// only a "macro redefined" warning (a -D on its command line, redefined in the source, keeps the
			// source's value from there on); two #defines in the source remain an error here.
			// Evidence: black-box observation of `xcrun metal -c -DFOO=1` on a source that has
			// "#define FOO 2", macOS 26.7.3, Apple metal 32023.883 (tier 2). Not documented.
			// Test: probe/pp_predefined_macro_redefined_by_source and the "a predefined macro the source
			// defines again" check in tests/preprocess_api.cpp.
			if (!old.predefined) {
				fail(nameLoc, "\"" + name + "\" redefined differently (first defined at "
					+ describe(old.definedAt) + ")");
			}
			macro->predefined = _definingPredefined;
			existing->second = std::move(macro);
			return;
		}
		return;
	}

	macro->predefined = _definingPredefined;
	_macros[name] = std::move(macro);
}

bool Preprocessor::isDefinedName(std::string_view name, const Loc& at) const {
	if (isUnsupportedBuiltin(name)) {
		fail(at, "\"" + std::string(name) + "\" is a builtin of Apple's compiler that mslc does not "
			"provide, so whether it is defined cannot be answered");
	}
	if (isAlternativeOperator(name)) {
		fail(at, "C++ operator '" + std::string(name) + "' used as a macro name");
	}
	return _macros.find(name) != _macros.end() || builtinByName(name) || name == "__has_include"
		|| name == "_Pragma";
}

} // namespace mslc
