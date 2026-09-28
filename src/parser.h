#pragma once

#include "ast.h"

#include <functional>
#include <initializer_list>
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

public:
	explicit Parser(std::vector<Token> tokens): _tokens(std::move(tokens)) {}

	TranslationUnit parse();

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
	void parsePreprocessorDirective();
	StructDecl parseStructDeclaration();
	FunctionDecl parseFunctionDeclaration(Stage stage);

	// Reads a bare identifier used as a name, such as a struct field's.
	void expectFieldName(std::string& out);

	// Types
	Type parseType();
	bool parseAddressSpace(AddressSpace& space);
	std::optional<uint32_t> tryParseArrayLength();

	// Parameters
	Parameter parseParameter();
	ParameterAttributes parseParameterAttributes();

	// One [[...]] attribute list, handing each attribute's name and its
	// optional constant integer argument to visit. Shared by parameters and
	// struct fields, since both spell the list the same way.
	void parseAttributeList(const std::function<void(const std::string&, std::optional<uint32_t>)>& visit);

	// Struct fields
	FieldAttributes parseFieldAttributes();

	// Statements
	StatementPtr parseStatement();
	StatementPtr parseCompoundStatement();
	StatementPtr parseIfStatement();
	StatementPtr parseForStatement();
	StatementPtr parseWhileStatement();
	StatementPtr parseReturnStatement();

	// Expressions, by precedence level.
	ExpressionPtr parseExpression();
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
