#pragma once

#include "ast.h"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <string_view>
#include <utility>

namespace mslc {

// True when a name is one of MSL's builtin attribute or builtin function names.
// Exposed because whether such a name is an error depends on scope, which only
// the emitter knows: a parameter is free to be called "position".
bool isMSLBuiltinName(std::string_view name);

// Parses an MSL token stream into a translation unit.
//
// The subset covers what the target corpus needs: struct declarations, kernel,
// vertex and fragment entry points, and the statement and expression forms
// those use. Anything outside it is a CompileError naming the construct, never
// a silent skip, so a caller cannot end up compiling a shader that quietly
// dropped a statement.
class Parser {
	std::vector<Token> _tokens;
	size_t _position = 0;
	TranslationUnit _unit;

	// Names the program has given a type with typedef, and the enums it declared.
	// An enum's constants are folded as they are read and spelled as integer
	// literals wherever they are used, so nothing after the parser sees them. The
	// enum types themselves are only remembered by name, so that using one as a
	// type is reported as what it is.
	std::map<std::string, Type> _typedefs;
	std::map<std::string, std::string> _enumTypes; // name or typedef name -> tag
	std::map<std::string, int64_t> _enumConstants;

	// Every file-scope name and what it names, so a typedef or an enum constant
	// that collides with another declaration is reported, as it is in Apple's
	// compiler, instead of the later one silently winning.
	std::map<std::string, std::string> _declared;

public:
	explicit Parser(std::vector<Token> tokens): _tokens(std::move(tokens)) {}

	TranslationUnit parse();

	// Index of the token the parser is at, which is the one a failure is about.
	size_t position() const { return _position; }

private:
	const Token& current() const { return _tokens[_position]; }
	const Token& lookahead(size_t offset = 1) const;
	TokenKind kind() const { return current().kind; }

	bool at(TokenKind expected) const { return kind() == expected; }

	// Consumes a token of the expected kind, or throws naming what was found.
	const Token& advance();
	void expect(TokenKind expected, const char* context);
	bool match(TokenKind expected);
	bool matchIdentifier(const char* text);

	// True when the current token is the given bare keyword.
	bool atKeyword(const char* text) const;
	const Token& expectKeyword(const char* text, const char* context);

	size_t line() const;

	// Top-level declarations
	void parseDeclaration();
	StructDecl parseStructDeclaration();
	void parseStructBody(StructDecl& decl);
	void parseTypedef();

	// "enum Tag { A, B = 2 }" up to and including the closing brace, or just
	// "enum Tag" when there is no brace, which says so in hadBody. Returns the
	// tag, empty for an enum without one.
	std::string parseEnumDeclaration(bool& hadBody);

	// Records a file-scope name, or throws when it is already taken by something
	// the typedef and enum support has to keep distinct from it.
	void declareName(const std::string& name, const char* what);

	// Throws when a variable or parameter would hide a typedef or an enum constant.
	void rejectShadowing(const std::string& name) const;

	// Resolves a scalar, vector, matrix or typedef name to its type.
	bool resolveTypeName(std::string_view text, Type& out) const;

	// A constant integer expression: literals, enum constants and the operators
	// on them. Anything that is not a constant is a CompileError.
	int64_t evaluateConstant(const Expression& expression, bool nested) const;
	int64_t parseConstantIndex(const std::string& context);
	FunctionDecl parseFunctionDeclaration(Stage stage);
	VariableDeclaration parseGlobalDeclaration();

	// Reads a bare identifier used as a name, such as a struct field's.
	void expectFieldName(std::string& out);

	// An initialiser, which is an expression or a braced list. A list is only
	// spelled in an initialiser, so it is not a primary expression.
	ExpressionPtr parseInitializer();
	ExpressionPtr parseInitializerList();

	// Types
	Type parseType();
	bool parseAddressSpace(AddressSpace& space);
	std::optional<uint32_t> tryParseArrayLength();

	// Parameters
	Parameter parseParameter();
	ParameterAttributes parseParameterAttributes();

	// Struct fields
	FieldAttributes parseFieldAttributes();

	// One [[...]] attribute list, handing each attribute's name and its optional
	// constant integer argument to visit. Parameters and struct fields spell the
	// list the same way, so they share the scanner.
	void parseAttributeList(const std::function<void(const std::string&, std::optional<uint32_t>)>& visit);

	// Statements
	StatementPtr parseStatement();
	StatementPtr parseCompoundStatement();
	StatementPtr parseIfStatement();
	StatementPtr parseForStatement();
	StatementPtr parseWhileStatement();
	StatementPtr parseReturnStatement();

	// Expressions, by precedence level.
	ExpressionPtr parseExpression();

	// The comma-separated list inside a pair of parentheses, up to and including
	// the closing one. A call and a constructor differ in what they are called,
	// not in how their arguments are separated.
	std::vector<ExpressionPtr> parseArgumentList(const char* closing);
	ExpressionPtr parseAssignment();
	ExpressionPtr parseLogicalOr();
	ExpressionPtr parseLogicalAnd();
	ExpressionPtr parseBitwiseOr();
	ExpressionPtr parseBitwiseXor();
	ExpressionPtr parseBitwiseAnd();
	ExpressionPtr parseEquality();
	ExpressionPtr parseRelational();
	ExpressionPtr parseShift();
	ExpressionPtr parseAdditive();
	ExpressionPtr parseMultiplicative();
	ExpressionPtr parseUnary();
	ExpressionPtr parsePostfix();
	ExpressionPtr parsePrimary();

	// One precedence level of left-associative binary operators. A member
	// rather than a free function so it can reach the level parsers, and a
	// template so each level names only its own operators.
	template <typename NextLevel>
	ExpressionPtr parseBinaryLevel(NextLevel next, std::initializer_list<std::pair<TokenKind, BinaryOperator>> operators) {
		auto left = next(*this);

		while (true) {
			BinaryOperator chosen = BinaryOperator::Add;
			bool matched = false;

			for (const auto& candidate: operators) {
				if (kind() == candidate.first) {
					chosen = candidate.second;
					matched = true;
					break;
				}
			}

			if (!matched) {
				return left;
			}

			advance();

			auto expression = std::make_unique<Expression>();
			expression->kind = ExpressionKind::Binary;
			expression->line = left->line;
			expression->binaryOperator = chosen;
			expression->left = std::move(left);
			expression->right = next(*this);

			left = std::move(expression);
		}
	}
};

}
