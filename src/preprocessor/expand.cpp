#include "preprocessor/internal.h"

namespace mslc {

namespace pp {

bool hideSetHas(const HideSet& set, const Macro* macro) {
	return set && std::binary_search(set->begin(), set->end(), macro, std::less<const Macro*>());
}

HideSet hideSetAdd(const HideSet& set, const Macro* macro) {
	auto result = std::make_shared<std::vector<const Macro*>>();
	if (set) {
		*result = *set;
	}
	result->insert(std::upper_bound(result->begin(), result->end(), macro, std::less<const Macro*>()), macro);
	return result;
}

HideSet hideSetUnion(const HideSet& a, const HideSet& b) {
	if (!b || b->empty() || a == b) {
		return a;
	}
	if (!a || a->empty()) {
		return b;
	}

	auto result = std::make_shared<std::vector<const Macro*>>();
	std::set_union(a->begin(), a->end(), b->begin(), b->end(), std::back_inserter(*result),
		std::less<const Macro*>());
	return result;
}

HideSet hideSetIntersect(const HideSet& a, const HideSet& b) {
	if (!a || !b) {
		return nullptr;
	}
	auto result = std::make_shared<std::vector<const Macro*>>();
	std::set_intersection(a->begin(), a->end(), b->begin(), b->end(), std::back_inserter(*result),
		std::less<const Macro*>());
	return result;
}

// "1L", "0b101" and "1e+5" are each one preprocessing number in C, though the
// lexer reads them as several tokens, so a paste that forms one is valid.
bool isPreprocessingNumber(std::string_view text) {
	size_t i = 0;
	if (i < text.size() && text[i] == '.') {
		++i;
	}
	if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) {
		return false;
	}

	for (++i; i < text.size(); ++i) {
		const char c = text[i];
		if ((c == '+' || c == '-') && std::string_view("eEpP").find(text[i - 1]) != std::string_view::npos) {
			continue;
		}
		if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.')) {
			return false;
		}
	}
	return true;
}

} // namespace pp

using namespace pp;

std::vector<Token> Preprocessor::lexSynthetic(const std::string& text) {
	_storage.push_back(text);
	std::vector<Token> tokens = tokenize(_storage.back(), true);
	tokens.pop_back();
	return tokens;
}

PPTok Preprocessor::synthesize(const std::string& text, const PPTok& like) {
	std::vector<Token> tokens = lexSynthetic(text);
	PPTok result = like;
	result.hide = nullptr;
	result.placemarker = false;
	result.tok = tokens.front();
	result.tok.spaceBefore = like.tok.spaceBefore;
	result.tok.startOfLine = like.tok.startOfLine;
	return result;
}

PPTok Preprocessor::stringify(const std::vector<PPTok>& argument, const PPTok& like, const Loc& at) {
	std::string text = "\"";

	// The lexer has no character literals, so a quote is tracked here: inside
	// one, as inside a string, a backslash or a double quote is escaped.
	bool inCharacter = false;
	for (size_t i = 0; i < argument.size(); ++i) {
		const Token& token = argument[i].tok;
		if (i > 0 && token.spaceBefore) {
			text += ' ';
		}

		const bool opensOrCloses = token.kind == TokenKind::Invalid && token.text == "'";
		if (opensOrCloses) {
			inCharacter = !inCharacter;
		}

		const bool quoted = isStringLiteral(token) || inCharacter || opensOrCloses
			|| (token.kind == TokenKind::Invalid && token.text[0] == '"');
		for (const char c: token.text) {
			if (quoted && (c == '"' || c == '\\')) {
				text += '\\';
			}
			text += c;
		}
	}
	text += '"';

	std::vector<Token> tokens = lexSynthetic(text);
	if (tokens.size() != 1 || tokens[0].kind == TokenKind::Invalid) {
		fail(at, "the argument cannot be stringified into a valid string literal");
	}

	PPTok result = like;
	result.hide = nullptr;
	result.placemarker = false;
	result.tok = tokens[0];
	result.tok.spaceBefore = like.tok.spaceBefore;
	result.tok.startOfLine = false;
	return result;
}

PPTok Preprocessor::pasteTokens(const PPTok& left, const PPTok& right, const Loc& at) {
	if (left.placemarker) {
		PPTok result = right;
		result.tok.spaceBefore = left.tok.spaceBefore;
		return result;
	}
	if (right.placemarker) {
		return left;
	}

	const std::string text = std::string(left.tok.text) + std::string(right.tok.text);
	std::vector<Token> tokens = lexSynthetic(text);

	bool valid = !tokens.empty();
	for (const Token& token: tokens) {
		valid = valid && token.kind != TokenKind::Invalid;
	}
	valid = valid && (tokens.size() == 1 || isPreprocessingNumber(text));
	if (!valid) {
		fail(at, "pasting \"" + std::string(left.tok.text) + "\" and \"" + std::string(right.tok.text)
			+ "\" does not give a valid preprocessing token");
	}

	// "2B" is one token in C, so it must not become the number 2 and the name B,
	// which could then be expanded. It is kept whole and diagnosed if it is used.
	if (tokens.size() > 1) {
		Token whole = tokens[0];
		whole.kind = TokenKind::Invalid;
		whole.invalidReason = InvalidReason::MalformedNumber;
		whole.text = _storage.back();
		tokens = { whole };
	}

	PPTok pasted = left;
	pasted.tok = tokens[0];
	pasted.tok.spaceBefore = left.tok.spaceBefore;
	pasted.tok.startOfLine = false;
	pasted.hide = hideSetIntersect(left.hide, right.hide);
	return pasted;
}

std::vector<PPTok> Preprocessor::substitute(const Macro& macro, const PPTok& name,
	const std::vector<std::vector<PPTok>>& arguments, bool variadicOmitted, const Loc& bodyLoc,
	size_t depth) {

	std::vector<std::optional<std::vector<PPTok>>> expandedArguments(arguments.size());
	auto expandedArgument = [&](size_t index) -> const std::vector<PPTok>& {
		if (!expandedArguments[index]) {
			std::deque<PPTok> work(arguments[index].begin(), arguments[index].end());
			std::vector<PPTok> out;
			scan(nullptr, work, out, depth + 1, true);
			expandedArguments[index] = std::move(out);
		}
		return *expandedArguments[index];
	};

	std::vector<PPTok> out;
	bool pasteNext = false;

	auto append = [&](std::vector<PPTok> contribution) {
		if (contribution.empty()) {
			return;
		}

		size_t from = 0;
		if (pasteNext) {
			pasteNext = false;
			out.back() = pasteTokens(out.back(), contribution[0], bodyLoc);
			from = 1;
		}

		for (size_t i = from; i < contribution.size(); ++i) {
			out.push_back(std::move(contribution[i]));
		}
	};

	auto bodyToken = [&](const BodyItem& item) {
		PPTok token;
		token.tok = item.tok;
		token.tok.spaceBefore = item.spaceBefore;
		token.tok.startOfLine = false;
		token.loc = bodyLoc;
		return token;
	};

	const std::vector<BodyItem>& body = macro.body;
	for (size_t k = 0; k < body.size(); ++k) {
		const BodyItem& item = body[k];

		switch (item.kind) {
			case BodyKind::Token:
				append({ bodyToken(item) });
				break;

			case BodyKind::Stringify:
				append({ stringify(arguments[item.param], bodyToken(item), bodyLoc) });
				break;

			case BodyKind::Paste: {
				// GNU: ", ## __VA_ARGS__" drops the comma when the variable arguments
				// are left out of the call, and otherwise pastes nothing. An empty one,
				// as in f(1,), keeps it, which is what Apple's compiler does.
				const bool commaBefore = !out.empty() && out.back().tok.kind == TokenKind::Comma
					&& !out.back().placemarker;
				const bool variadicNext = macro.variadic && k + 1 < body.size()
					&& body[k + 1].kind == BodyKind::Param && body[k + 1].param == macro.params.size();
				if (commaBefore && variadicNext && body[k - 1].kind == BodyKind::Token) {
					if (variadicOmitted) {
						out.pop_back();
						++k;
					}
					break;
				}
				pasteNext = true;
				break;
			}

			case BodyKind::Param: {
				const bool pasted = pasteNext || (k + 1 < body.size() && body[k + 1].kind == BodyKind::Paste);
				std::vector<PPTok> contribution = pasted ? arguments[item.param] : expandedArgument(item.param);

				if (contribution.empty()) {
					if (pasted) {
						PPTok marker;
						marker.placemarker = true;
						marker.tok.spaceBefore = item.spaceBefore;
						marker.loc = bodyLoc;
						contribution.push_back(std::move(marker));
					}
				} else {
					contribution[0].tok.spaceBefore = item.spaceBefore;
				}
				append(std::move(contribution));
				break;
			}
		}
	}

	out.erase(std::remove_if(out.begin(), out.end(), [](const PPTok& t) { return t.placemarker; }), out.end());

	if (!out.empty()) {
		out[0].tok.spaceBefore = name.tok.spaceBefore;
		out[0].tok.startOfLine = name.tok.startOfLine;
	}
	for (size_t i = 1; i < out.size(); ++i) {
		out[i].tok.startOfLine = false;
	}
	return out;
}

std::vector<PPTok> Preprocessor::expandBuiltin(Builtin builtin, const PPTok& name) {
	switch (builtin) {
		case Builtin::Line:
			return { synthesize(std::to_string(name.loc.presumedLine), name) };
		case Builtin::Counter:
			return { synthesize(std::to_string(_counter++), name) };
		case Builtin::IncludeLevel:
			return { synthesize(std::to_string(_includes.size()), name) };
		case Builtin::File:
		case Builtin::BaseFile: {
			const std::string& path = builtin == Builtin::File ? _names[name.loc.nameId] : _basePath;
			std::string literal = "\"";
			for (const char c: path) {
				if (c == '"' || c == '\\') {
					literal += '\\';
				}
				literal += c;
			}
			literal += '"';
			return { synthesize(literal, name) };
		}
	}
	return {};
}

void Preprocessor::scan(Cursor* cursor, std::deque<PPTok>& work, std::vector<PPTok>& out, size_t depth,
	bool keepPragma) {
	if (depth > kMaxExpansionDepth) {
		throw CompileError("macro arguments nested more than " + std::to_string(kMaxExpansionDepth)
			+ " deep");
	}

	// Pulls the next token for an argument list: pending rescan tokens first,
	// then the file. A directive inside an argument list is refused, because the
	// reference compiler's behaviour there is not portable.
	auto pull = [&](PPTok& token, const Loc& macroLoc, const std::string& macroName) {
		if (!work.empty()) {
			token = std::move(work.front());
			work.pop_front();
			return;
		}

		if (cursor) {
			const Token& raw = cursor->file.tokens[cursor->index];
			if (raw.kind == TokenKind::Hash && raw.startOfLine) {
				fail(locate(*cursor, raw), "a preprocessor directive inside the arguments of macro \""
					+ macroName + "\" is not supported");
			}
			if (raw.kind != TokenKind::EndOfFile) {
				token = fromFile(*cursor, raw);
				++cursor->index;
				return;
			}
		}

		fail(macroLoc, "unterminated argument list invoking macro \"" + macroName + "\"");
	};

	while (true) {
		PPTok token;

		// Handed over in batches so that one expansion that produces a great many
		// tokens reaches the output limit instead of exhausting memory first.
		if (cursor && out.size() >= 4096) {
			flush(out);
		}

		if (!work.empty()) {
			token = std::move(work.front());
			work.pop_front();
		} else {
			if (!cursor) {
				return;
			}

			flush(out);

			const Token& raw = cursor->file.tokens[cursor->index];
			if (raw.kind == TokenKind::EndOfFile) {
				return;
			}
			if (raw.kind == TokenKind::Hash && raw.startOfLine) {
				directive(*cursor);
				continue;
			}
			if (!cursor->conditionals.empty() && !cursor->conditionals.back().live) {
				++cursor->index;
				continue;
			}

			token = fromFile(*cursor, raw);
			++cursor->index;
		}

		if (!isName(token.tok)) {
			out.push_back(std::move(token));
			continue;
		}

		const std::string_view name = token.tok.text;

		if (const auto builtin = builtinByName(name)) {
			for (PPTok& expanded: expandBuiltin(*builtin, token)) {
				out.push_back(std::move(expanded));
			}
			continue;
		}
		if (name == "_Pragma") {
			// The operator spelling of #pragma: the same advice, dropped the same way. A
			// pragma that changes which macros exist is refused here as it is there.
			PPTok open, operand, close;
			pull(open, token.loc, "_Pragma");
			if (open.tok.kind != TokenKind::LParen) {
				fail(token.loc, "_Pragma takes a parenthesised string literal");
			}
			pull(operand, token.loc, "_Pragma");
			pull(close, token.loc, "_Pragma");
			if (!isStringLiteral(operand.tok) || close.tok.kind != TokenKind::RParen) {
				fail(token.loc, "_Pragma takes a parenthesised string literal");
			}
			// While a macro argument is pre-expanded the pragma is kept, so that the rescan
			// of the substituted body, which has the file, acts on it (C11 6.10.3.4).
			if (keepPragma && !cursor) {
				out.push_back(std::move(token));
				out.push_back(std::move(open));
				out.push_back(std::move(operand));
				out.push_back(std::move(close));
				continue;
			}
			const std::string_view text = operand.tok.text;
			const size_t start = text.find_first_not_of(" \t", text.find('"') + 1);
			if (start != std::string_view::npos) {
				const size_t end = text.find_first_not_of(
					"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", start);
				applyPragma(cursor, text.substr(start, end == std::string_view::npos ? end : end - start),
					token.loc);
			}
			continue;
		}
		if (name == "__has_include" || isUnsupportedBuiltin(name)) {
			fail(token.loc, name == "__has_include"
				? "__has_include can only be used in an #if or #elif"
				: "\"" + std::string(name) + "\" is a builtin of Apple's compiler that mslc does not provide");
		}

		const auto found = _macros.find(name);
		if (found == _macros.end() || hideSetHas(token.hide, found->second.get())) {
			out.push_back(std::move(token));
			continue;
		}

		const Macro& macro = *found->second;
		std::vector<std::vector<PPTok>> arguments;
		size_t heldHere = 0;
		bool variadicOmitted = false;
		HideSet hide;
		Loc bodyLoc = token.loc;

		if (!macro.functionLike) {
			hide = hideSetAdd(token.hide, &macro);
		} else {
			bool opens = false;
			if (!work.empty()) {
				opens = work.front().tok.kind == TokenKind::LParen;
			} else if (cursor) {
				opens = cursor->file.tokens[cursor->index].kind == TokenKind::LParen;
			}
			if (!opens) {
				out.push_back(std::move(token));
				continue;
			}

			PPTok open;
			pull(open, token.loc, macro.name);

			const size_t named = macro.params.size();
			arguments.emplace_back();
			size_t nesting = 0;
			PPTok close;

			while (true) {
				PPTok next;
				pull(next, token.loc, macro.name);

				// Every invocation being expanded keeps its arguments, and each nested one
				// copies what is inside it, so what is held at once is bounded as a whole:
				// by the time a nesting limit fires, the copies would already be too many.
				if (++_heldArguments > kMaxHeldArgumentTokens) {
					fail(token.loc, "the arguments of macro calls being expanded hold more than "
						+ std::to_string(kMaxHeldArgumentTokens) + " tokens");
				}
				++heldHere;

				if (next.tok.kind == TokenKind::LParen) {
					++nesting;
				} else if (next.tok.kind == TokenKind::RParen) {
					if (nesting == 0) {
						close = std::move(next);
						break;
					}
					--nesting;
				} else if (next.tok.kind == TokenKind::Comma && nesting == 0
					&& !(macro.variadic && arguments.size() == named + 1)) {
					arguments.emplace_back();
					continue;
				}

				arguments.back().push_back(std::move(next));
			}

			// f() is one empty argument to a macro that takes one, and none to a
			// macro that takes none or only variable arguments.
			if (named == 0 && arguments.size() == 1 && arguments[0].empty()) {
				arguments.clear();
			}
			if (macro.variadic && arguments.size() == named) {
				variadicOmitted = true;
				arguments.emplace_back();
			}

			const size_t wanted = named + (macro.variadic ? 1 : 0);
			if (arguments.size() != wanted) {
				fail(token.loc, "macro \"" + macro.name + "\" requires " + std::to_string(named)
					+ (macro.variadic ? " or more" : "") + " arguments, but " + std::to_string(arguments.size())
					+ (arguments.size() == 1 ? " was" : " were") + " given");
			}

			hide = hideSetAdd(hideSetIntersect(token.hide, close.hide), &macro);
			bodyLoc = close.loc;
		}

		std::vector<PPTok> result = substitute(macro, token, arguments, variadicOmitted, bodyLoc, depth);
		_heldArguments -= heldHere;

		_expanded += result.size();
		if (_expanded > kMaxExpandedTokens) {
			fail(token.loc, "macro expansion produces more than " + std::to_string(kMaxExpandedTokens)
				+ " tokens");
		}

		for (PPTok& t: result) {
			t.hide = hideSetUnion(t.hide, hide);
		}
		for (auto it = result.rbegin(); it != result.rend(); ++it) {
			work.push_front(std::move(*it));
		}

		if (work.size() + out.size() > kMaxPendingTokens) {
			fail(token.loc, "macro expansion holds more than " + std::to_string(kMaxPendingTokens)
				+ " tokens at once");
		}
	}
}

} // namespace mslc
