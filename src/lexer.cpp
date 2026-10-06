#include "lexer.h"

#include <cctype>
#include <cstdlib>
#include <unordered_map>

namespace mslc {

const char* tokenKindName(TokenKind kind) {
	switch (kind) {
		case TokenKind::EndOfFile: return "end of file";
		case TokenKind::Identifier: return "identifier";
		case TokenKind::IntegerLiteral: return "integer literal";
		case TokenKind::FloatLiteral: return "float literal";
		case TokenKind::LBrace: return "{";
		case TokenKind::RBrace: return "}";
		case TokenKind::LParen: return "(";
		case TokenKind::RParen: return ")";
		case TokenKind::LBracket: return "[";
		case TokenKind::RBracket: return "]";
		case TokenKind::Semicolon: return ";";
		case TokenKind::Comma: return ",";
		case TokenKind::Colon: return ":";
		case TokenKind::ColonColon: return "::";
		case TokenKind::Dot: return ".";
		case TokenKind::Arrow: return "->";
		case TokenKind::Plus: return "+";
		case TokenKind::Minus: return "-";
		case TokenKind::Star: return "*";
		case TokenKind::Slash: return "/";
		case TokenKind::Percent: return "%";
		case TokenKind::Assign: return "=";
		case TokenKind::PlusAssign: return "+=";
		case TokenKind::MinusAssign: return "-=";
		case TokenKind::StarAssign: return "*=";
		case TokenKind::SlashAssign: return "/=";
		case TokenKind::PercentAssign: return "%=";
		case TokenKind::AmpersandAssign: return "&=";
		case TokenKind::PipeAssign: return "|=";
		case TokenKind::CaretAssign: return "^=";
		case TokenKind::ShiftLeftAssign: return "<<=";
		case TokenKind::ShiftRightAssign: return ">>=";
		case TokenKind::Less: return "<";
		case TokenKind::LessEqual: return "<=";
		case TokenKind::Greater: return ">";
		case TokenKind::GreaterEqual: return ">=";
		case TokenKind::Equal: return "==";
		case TokenKind::NotEqual: return "!=";
		case TokenKind::Ampersand: return "&";
		case TokenKind::Pipe: return "|";
		case TokenKind::Caret: return "^";
		case TokenKind::Tilde: return "~";
		case TokenKind::Bang: return "!";
		case TokenKind::AndAnd: return "&&";
		case TokenKind::OrOr: return "||";
		case TokenKind::ShiftLeft: return "<<";
		case TokenKind::ShiftRight: return ">>";
		case TokenKind::Increment: return "++";
		case TokenKind::Decrement: return "--";
		case TokenKind::Hash: return "#";
		case TokenKind::HashHash: return "##";
		case TokenKind::Ellipsis: return "...";
		case TokenKind::Question: return "?";
		case TokenKind::Invalid: return "invalid token";
	}

	return "token";
}

namespace {

	bool isIdentifierStart(char c) {
		return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
	}

	bool isIdentifierContinue(char c) {
		return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
	}

	// Longest-match table for the multi-character operators. Without this the
	// "<=" would lex as "<" then "=", which is fine for a parser that is
	// already case-by-case, but keeping the lexer maximal means the parser
	// never has to backtrack.
	const std::unordered_map<std::string_view, TokenKind>& multiCharOperators() {
		static const std::unordered_map<std::string_view, TokenKind> table = {
			{ "<<=", TokenKind::ShiftLeftAssign },
			{ ">>=", TokenKind::ShiftRightAssign },
			{ "->", TokenKind::Arrow },
			{ "::", TokenKind::ColonColon },
			{ "++", TokenKind::Increment },
			{ "--", TokenKind::Decrement },
			{ "<=", TokenKind::LessEqual },
			{ ">=", TokenKind::GreaterEqual },
			{ "==", TokenKind::Equal },
			{ "!=", TokenKind::NotEqual },
			{ "&&", TokenKind::AndAnd },
			{ "||", TokenKind::OrOr },
			{ "<<", TokenKind::ShiftLeft },
			{ ">>", TokenKind::ShiftRight },
			{ "+=", TokenKind::PlusAssign },
			{ "-=", TokenKind::MinusAssign },
			{ "*=", TokenKind::StarAssign },
			{ "/=", TokenKind::SlashAssign },
			{ "%=", TokenKind::PercentAssign },
			{ "&=", TokenKind::AmpersandAssign },
			{ "|=", TokenKind::PipeAssign },
			{ "^=", TokenKind::CaretAssign },
		};

		return table;
	}

}

std::string describeInvalidToken(const Token& token) {
	const std::string where = " at offset " + std::to_string(token.offset);

	switch (token.invalidReason) {
		case InvalidReason::UnexpectedCharacter:
			return "unexpected character '" + std::string(token.text) + "'" + where;
		case InvalidReason::NewlineInString:
			return "newline in a string literal" + where;
		case InvalidReason::UnterminatedString:
			return "unterminated string literal" + where;
		case InvalidReason::MalformedNumber:
			return "invalid numeric literal \"" + std::string(token.text) + "\"";
		case InvalidReason::None:
			break;
	}

	return "invalid token" + where;
}

std::vector<Token> tokenize(std::string_view source, bool lenient) {
	std::vector<Token> tokens;

	size_t i = 0;
	const size_t size = source.size();

	// Incremented wherever a newline is consumed, so a token can say which line
	// it started on.
	size_t line = 1;

	// A newline outside a comment starts a new logical line; whitespace and
	// comments separate the tokens either side of them.
	bool atLineStart = true;
	bool spaceBefore = false;

	auto place = [&](Token& token) {
		token.startOfLine = atLineStart;
		token.spaceBefore = spaceBefore;
		atLineStart = false;
		spaceBefore = false;
	};

	auto makeToken = [&](TokenKind kind, size_t start) {
		Token token;
		token.kind = kind;
		token.offset = start;
		token.line = line;
		token.text = source.substr(start, i - start);
		place(token);
		tokens.push_back(token);
	};

	// reported is the offset the diagnostic names, which is not always where
	// the bad text starts.
	auto invalid = [&](InvalidReason reason, size_t start, size_t reported) {
		Token token;
		token.kind = TokenKind::Invalid;
		token.invalidReason = reason;
		token.offset = reported;
		token.line = line;
		token.text = source.substr(start, i - start);
		if (!lenient) {
			throw CompileError(describeInvalidToken(token));
		}
		place(token);
		tokens.push_back(token);
	};

	while (i < size) {
		const char c = source[i];

		// whitespace
		if (std::isspace(static_cast<unsigned char>(c))) {
			if (c == '\n') {
				++line;
				atLineStart = true;
			}
			spaceBefore = true;
			++i;
			continue;
		}

		// line comment
		if (c == '/' && i + 1 < size && source[i + 1] == '/') {
			while (i < size && source[i] != '\n') {
				++i;
			}
			spaceBefore = true;
			continue;
		}

		// block comment
		if (c == '/' && i + 1 < size && source[i + 1] == '*') {
			const size_t start = i;
			i += 2;

			bool closed = false;
			while (i + 1 < size) {
				if (source[i] == '*' && source[i + 1] == '/') {
					i += 2;
					closed = true;
					break;
				}
				if (source[i] == '\n') {
					++line;
				}
				++i;
			}

			if (!closed) {
				i = size;
				throw LexError("unterminated block comment starting at offset " + std::to_string(start), start);
			}
			spaceBefore = true;
			continue;
		}

		// string literal, kept as a single token; the subset has no use for it
		// yet but swallowing it here stops the contents being lexed as code
		if (c == '"') {
			const size_t start = i;
			++i;

			bool closed = false;
			bool newlineEnded = false;
			while (i < size) {
				if (source[i] == '\\' && i + 1 < size) {
					// A backslash-newline splices two physical lines into one
					// logical line, so the logical line does not advance here.
					// That is the whole point of a splice, and it is what lets a
					// directive that owns a spliced string still end at the end
					// of that logical line.
					i += 2;
					continue;
				}
				if (source[i] == '\n') {
					// A raw newline is not legal in a string, and Apple's compiler
					// rejects it. Reporting it keeps the line count meaningful
					// rather than guessing where the token was meant to end.
					invalid(InvalidReason::NewlineInString, start, i);
					newlineEnded = true;
					break;
				}
				if (source[i] == '"') {
					++i;
					closed = true;
					break;
				}
				++i;
			}

			if (newlineEnded) {
				continue;
			}

			if (!closed) {
				invalid(InvalidReason::UnterminatedString, start, start);
				continue;
			}

			makeToken(TokenKind::Identifier, start);
			continue;
		}

		// numeric literal
		if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < size && std::isdigit(static_cast<unsigned char>(source[i + 1])))) {
			const size_t start = i;
			bool isFloat = false;

			if (c == '0' && i + 1 < size && (source[i + 1] == 'x' || source[i + 1] == 'X')) {
				i += 2;
				while (i < size && std::isxdigit(static_cast<unsigned char>(source[i]))) {
					++i;
				}
			} else {
				while (i < size && std::isdigit(static_cast<unsigned char>(source[i]))) {
					++i;
				}

				if (i < size && source[i] == '.') {
					isFloat = true;
					++i;
					while (i < size && std::isdigit(static_cast<unsigned char>(source[i]))) {
						++i;
					}
				}

				if (i < size && (source[i] == 'e' || source[i] == 'E')) {
					isFloat = true;
					++i;
					if (i < size && (source[i] == '+' || source[i] == '-')) {
						++i;
					}
					while (i < size && std::isdigit(static_cast<unsigned char>(source[i]))) {
						++i;
					}
				}
			}

			// MSL numeric suffixes
			bool isUnsigned = false;
			bool isHalf = false;
			if (i < size) {
				const char suffix = source[i];
				if (suffix == 'u' || suffix == 'U') {
					isUnsigned = true;
					isFloat = false;
					++i;
				} else if (suffix == 'f' || suffix == 'F' || suffix == 'h' || suffix == 'H') {
					isFloat = true;
					isHalf = suffix == 'h' || suffix == 'H';
					++i;
				}
			}

			Token token;
			token.offset = start;
			token.line = line;
			token.text = source.substr(start, i - start);
			token.integerIsUnsigned = isUnsigned;
			token.floatIsHalf = isHalf;
			place(token);

			if (isFloat) {
				token.kind = TokenKind::FloatLiteral;
				token.floatValue = std::strtod(std::string(token.text).c_str(), nullptr);
			} else {
				token.kind = TokenKind::IntegerLiteral;
				token.integerValue = std::strtoull(std::string(token.text).c_str(), nullptr, 0);
			}

			tokens.push_back(token);
			continue;
		}

		// identifier or keyword
		if (isIdentifierStart(c)) {
			const size_t start = i;
			while (i < size && isIdentifierContinue(source[i])) {
				++i;
			}

			makeToken(TokenKind::Identifier, start);
			continue;
		}

		// operator, longest match first
		{
			const auto& table = multiCharOperators();
			bool matched = false;

			for (size_t length = 4; length >= 2 && !matched; --length) {
				if (i + length > size) {
					continue;
				}

				auto it = table.find(source.substr(i, length));
				if (it != table.end()) {
					i += length;
					makeToken(it->second, i - length);
					matched = true;
				}
			}

			if (matched) {
				continue;
			}
		}

		{
			const size_t start = i;
			++i;

			switch (c) {
				case '{': makeToken(TokenKind::LBrace, start); continue;
				case '}': makeToken(TokenKind::RBrace, start); continue;
				case '(': makeToken(TokenKind::LParen, start); continue;
				case ')': makeToken(TokenKind::RParen, start); continue;
				case '[': makeToken(TokenKind::LBracket, start); continue;
				case ']': makeToken(TokenKind::RBracket, start); continue;
				case ';': makeToken(TokenKind::Semicolon, start); continue;
				case ',': makeToken(TokenKind::Comma, start); continue;
				case ':': makeToken(TokenKind::Colon, start); continue;
				case '.':
					if (source.substr(start, 3) == "...") {
						i = start + 3;
						makeToken(TokenKind::Ellipsis, start);
					} else {
						makeToken(TokenKind::Dot, start);
					}
					continue;
				case '+': makeToken(TokenKind::Plus, start); continue;
				case '-': makeToken(TokenKind::Minus, start); continue;
				case '*': makeToken(TokenKind::Star, start); continue;
				case '/': makeToken(TokenKind::Slash, start); continue;
				case '%': makeToken(TokenKind::Percent, start); continue;
				case '=': makeToken(TokenKind::Assign, start); continue;
				case '<': makeToken(TokenKind::Less, start); continue;
				case '>': makeToken(TokenKind::Greater, start); continue;
				case '&': makeToken(TokenKind::Ampersand, start); continue;
				case '|': makeToken(TokenKind::Pipe, start); continue;
				case '^': makeToken(TokenKind::Caret, start); continue;
				case '~': makeToken(TokenKind::Tilde, start); continue;
				case '!': makeToken(TokenKind::Bang, start); continue;
				case '#':
					if (source.substr(start, 2) == "##") {
						i = start + 2;
						makeToken(TokenKind::HashHash, start);
					} else {
						makeToken(TokenKind::Hash, start);
					}
					continue;
				case '?': makeToken(TokenKind::Question, start); continue;
				default: break;
			}
		}

		invalid(InvalidReason::UnexpectedCharacter, i - 1, i - 1);
	}

	Token end;
	end.kind = TokenKind::EndOfFile;
	end.offset = size;
	end.line = line;
	end.text = source.substr(size);
	tokens.push_back(end);

	return tokens;
}

}
