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

	// punctuation and operators
	LBrace, RBrace,
	LParen, RParen,
	LBracket, RBracket,
	Semicolon, Comma,
	Colon, ColonColon,
	Dot, Arrow,

	Plus, Minus, Star, Slash, Percent,
	Assign,
	PlusAssign, MinusAssign, StarAssign, SlashAssign, PercentAssign,
	AmpersandAssign, PipeAssign, CaretAssign, ShiftLeftAssign, ShiftRightAssign,
	Less, LessEqual,
	Greater, GreaterEqual,
	Equal, NotEqual,
	Ampersand, Pipe, Caret, Tilde, Bang,
	AndAnd, OrOr,
	ShiftLeft, ShiftRight,
	Increment, Decrement,

	// preprocessor
	Hash, HashHash,
	Ellipsis,
	Question,

	// Something the lexer could not read as a token. Only produced in lenient
	// mode, where it lets a preprocessor skip over text it is not going to use.
	Invalid,
};

enum class InvalidReason {
	None,
	UnexpectedCharacter,
	NewlineInString,
	UnterminatedString,

	// Made by the preprocessor, not the lexer: a number pasted together from
	// pieces, such as 2 ## B, which C reads as one token that is no literal.
	MalformedNumber,
};

struct Token {
	TokenKind kind = TokenKind::EndOfFile;
	std::string_view text;

	// literal payloads, valid for IntegerLiteral and FloatLiteral
	uint64_t integerValue = 0;
	double floatValue = 0.0;
	bool integerIsUnsigned = false;
	// How many l's the suffix has: a long is 1 and a long long 2.
	size_t integerLongCount = 0;
	// The suffix is not one the language has, such as lL or uu.
	bool integerSuffixInvalid = false;
	// Written in decimal, which is what keeps an unsuffixed literal out of the
	// unsigned types; hex and octal may take them.
	bool integerIsDecimal = true;
	// The value did not fit in 64 bits, and integerValue is not it.
	bool integerOverflows = false;
	// A digit the base does not have (the 9 in 09), or none at all (0x).
	bool integerDigitsInvalid = false;
	// A float literal written with an `h` suffix, which is a half rather than a float.
	bool floatIsHalf = false;

	// byte offset of the token start, for diagnostics
	size_t offset = 0;

	// 1-based line the token starts on. A preprocessor directive runs to the
	// end of its own line, which is the only place line structure matters, but
	// the AST records this for diagnostics too.
	size_t line = 1;

	// First token on its logical line: a '#' here begins a directive. A block
	// comment that spans lines does not end the line it started on.
	bool startOfLine = false;

	// Whitespace or a comment came before this token, which separates it from
	// the previous one when text is rebuilt from tokens and is what tells
	// "#define F(x)" from "#define F (x)".
	bool spaceBefore = false;

	// Why an Invalid token is invalid; for any other kind, None. The token's
	// offset is where the problem is reported.
	InvalidReason invalidReason = InvalidReason::None;
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

// A CompileError that knows where in the source it happened, for a caller that
// can turn the offset into a file:line:col.
class LexError: public CompileError {
	size_t _offset;

public:
	LexError(std::string message, size_t offset): CompileError(std::move(message)), _offset(offset) {}

	size_t offset() const { return _offset; }
};

// Converts MSL source to a token stream.
//
// Handles line and block comments, the integer and float literal forms MSL
// uses, and the [[attribute]] brackets. Preprocessor directives are tokenised
// as a Hash followed by whatever follows; the preprocessor interprets them.
//
// A character or string the lexer cannot read throws, unless lenient: then it
// becomes an Invalid token, so that text a preprocessor skips (an #if 0 block,
// an #error message with an apostrophe in it) is not diagnosed. Whatever is
// kept is diagnosed with describeInvalidToken. An unterminated block comment
// always throws, because a comment is recognised even in skipped text.
std::vector<Token> tokenize(std::string_view source, bool lenient = false);

// The diagnostic for an Invalid token.
std::string describeInvalidToken(const Token& token);

// Human-readable name for a token kind, for diagnostics.
const char* tokenKindName(TokenKind kind);

}
