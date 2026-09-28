#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mslc {

enum class TokenKind {
	EndOfFile,

	Identifier,
	IntegerLiteral,
	FloatLiteral,
	// A string literal. MSL has no use for one in an expression, but an
	// #include's header name is written as one, so it has to be recognisable
	// rather than lexed as the identifier it looks like.
	StringLiteral,

	// punctuation and operators
	LBrace, RBrace,
	LParen, RParen,
	LBracket, RBracket,
	Semicolon, Comma,
	Colon, ColonColon,
	Dot, Arrow,

	Plus, Minus, Star, Slash, Percent,
	Assign,
	PlusAssign, MinusAssign, StarAssign, SlashAssign,
	Less, LessEqual,
	Greater, GreaterEqual,
	Equal, NotEqual,
	Ampersand, Pipe, Caret, Tilde, Bang,
	AndAnd, OrOr,
	ShiftLeft, ShiftRight,
	Increment, Decrement,

	// preprocessor
	Hash,
};

struct Token {
	TokenKind kind = TokenKind::EndOfFile;
	std::string_view text;

	// literal payloads, valid for IntegerLiteral and FloatLiteral
	uint64_t integerValue = 0;
	double floatValue = 0.0;
	bool integerIsUnsigned = false;

	// byte offset of the token start, for diagnostics
	size_t offset = 0;
};

// Thrown by the lexer and parser for input mslc cannot represent. Carries a
// message that names the construct, so a caller can report something useful
// rather than "compilation failed".
class CompileError: public std::exception {
	std::string _message;

public:
	explicit CompileError(std::string message): _message(std::move(message)) {}

	const char* what() const noexcept override { return _message.c_str(); }
};

// Converts MSL source to a token stream.
//
// Handles line and block comments, the integer and float literal forms MSL
// uses, and the [[attribute]] brackets. Preprocessor directives are tokenised
// as a Hash followed by whatever follows; interpreting them is the parser's
// job, so that a caller can see them and decide.
std::vector<Token> tokenize(std::string_view source);

// Human-readable name for a token kind, for diagnostics.
const char* tokenKindName(TokenKind kind);

}
