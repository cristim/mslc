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
			{ "<<=", TokenKind::ShiftLeft },
			{ ">>=", TokenKind::ShiftRight },
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
		};

		return table;
	}

}

std::vector<Token> tokenize(std::string_view source) {
	std::vector<Token> tokens;

	size_t i = 0;
	const size_t size = source.size();

	auto makeToken = [&](TokenKind kind, size_t start) {
		Token token;
		token.kind = kind;
		token.offset = start;
		token.text = source.substr(start, i - start);
		tokens.push_back(token);
	};

	while (i < size) {
		const char c = source[i];

		// whitespace
		if (std::isspace(static_cast<unsigned char>(c))) {
			++i;
			continue;
		}

		// line comment
		if (c == '/' && i + 1 < size && source[i + 1] == '/') {
			while (i < size && source[i] != '\n') {
				++i;
			}
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
				++i;
			}

			if (!closed) {
				i = size;
				throw CompileError("unterminated block comment starting at offset " + std::to_string(start));
			}
			continue;
		}

		// string literal, kept as a single token; the subset has no use for it
		// yet but swallowing it here stops the contents being lexed as code
		if (c == '"') {
			const size_t start = i;
			++i;

			bool closed = false;
			while (i < size) {
				if (source[i] == '\\' && i + 1 < size) {
					i += 2;
					continue;
				}
				if (source[i] == '"') {
					++i;
					closed = true;
					break;
				}
				++i;
			}

			if (!closed) {
				throw CompileError("unterminated string literal at offset " + std::to_string(start));
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
			if (i < size) {
				const char suffix = source[i];
				if (suffix == 'u' || suffix == 'U') {
					isUnsigned = true;
					isFloat = false;
					++i;
				} else if (suffix == 'f' || suffix == 'F' || suffix == 'h' || suffix == 'H') {
					isFloat = true;
					++i;
				}
			}

			Token token;
			token.offset = start;
			token.text = source.substr(start, i - start);
			token.integerIsUnsigned = isUnsigned;

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
				case '.': makeToken(TokenKind::Dot, start); continue;
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
				case '#': makeToken(TokenKind::Hash, start); continue;
				default: break;
			}
		}

		throw CompileError(std::string("unexpected character '") + c + "' at offset "
			+ std::to_string(i - 1));
	}

	Token end;
	end.kind = TokenKind::EndOfFile;
	end.offset = size;
	end.text = source.substr(size);
	tokens.push_back(end);

	return tokens;
}

}
