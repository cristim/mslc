#pragma once

#include "ast.h"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <set>
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
	struct EnumConstant {
		int64_t value;
		ScalarKind kind; // the underlying type of the enum it belongs to
	};
	std::map<std::string, EnumConstant> _enumConstants;
	std::map<std::string, ScalarKind> _enumUnderlying; // tag or anonymous typedef name -> underlying type
	ScalarKind _lastEnumUnderlying = ScalarKind::Int;

	// Every file-scope name and what it names, so a typedef or an enum constant
	// that collides with another declaration is reported, as it is in Apple's
	// compiler, instead of the later one silently winning.
	std::map<std::string, std::string> _declared;

	// The namespaces the file declares. A declaration inside one is stored under
	// its qualified name, "A::B::x", in every table above, so a name lookup is a
	// string lookup and a clash is found by declareName as it is at file scope.
	// A scope is keyed by its qualified name, "" being the file; it holds the
	// namespaces a using directive named and the names a using declaration brought in.
	struct NamespaceScope {
		std::vector<std::string> directives;
		std::map<std::string, std::string> declarations;
	};
	std::vector<std::string> _namespacePath;
	std::set<std::string> _namespaces;
	std::map<std::string, NamespaceScope> _scopes;

	// The names declared in the function being parsed, which hide a file-scope
	// name of the same spelling that a namespace would otherwise resolve.
	std::vector<std::set<std::string>> _locals;

	// Levels of brackets, blocks and unbraced bodies the parser is inside, one
	// per recursive call. Each structured construct the emitter writes (an if, a
	// loop, the right operand of && or ||) needs at least one of them, so the cap
	// also keeps spirv-val's limit of 1023 nested constructs out of reach.
	uint32_t _nesting = 0;

public:
	// Apple's compiler stops at 256 of each of ( [ and { nested, so this is above
	// the 767 a program it accepts can reach. It is under spirv-val's 1023 and
	// leaves the parser's own stack a wide margin.
	static constexpr uint32_t kMaxNestingDepth = 1000;
	// Taller than any tree a source Apple accepts is likely to need, and a third
	// of what the emitter and the tree's destructor can recurse through on an
	// 8 MiB stack. Reached only by operators chained one after another, since
	// nesting is capped by kMaxNestingDepth.
	static constexpr uint32_t kMaxExpressionHeight = 4000;

	explicit Parser(std::vector<Token> tokens): _tokens(std::move(tokens)) {}

	TranslationUnit parse();

	// Index of the token the parser is at, which is the one a failure is about.
	size_t position() const { return _position; }

private:
	class NestingScope {
		Parser& _parser;

	public:
		explicit NestingScope(Parser& parser);
		~NestingScope() { --_parser._nesting; }
		NestingScope(const NestingScope&) = delete;
		NestingScope& operator=(const NestingScope&) = delete;
	};

	// Sets the height of an expression whose children are all in place.
	void measure(Expression& expression) const;

	// The body of an if, else, for or while: a block counts itself, anything else
	// counts here.
	StatementPtr parseBody();

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
	bool parseStructFunction(StructDecl& decl);
	void finishStructFunctions(const StructDecl& decl);
	void parseStructMember(const StructDecl& decl, size_t start);
	static std::string unqualifiedName(const std::string& name);
	void parseHelperParameters(FunctionDecl& decl, const std::string& quoted);
	ExpressionPtr constructWithConstructor(const Type& type, std::vector<ExpressionPtr>& arguments, size_t atLine);

	// What a struct's constructors lower to. A constructor with nothing to do is
	// no helper at all; one with something to do is the helper that returns the
	// built object, and a struct has at most one.
	struct ConstructorInfo {
		std::string helper;
		size_t parameters = 0;
		bool hasDefault = false;
	};
	std::map<std::string, ConstructorInfo> _constructors;
	std::vector<size_t> _pendingMembers;
	// The fields of the struct whose member function is being parsed: a bare use of
	// one means this.field.
	const std::set<std::string>* _memberFields = nullptr;
	// The member functions of the struct being read, by name, and whether each is static.
	std::map<std::string, bool> _memberFunctions;
	std::string _memberStruct;
	void parseTypedef();

	// "enum Tag { A, B = 2 }" up to and including the closing brace, or just
	// "enum Tag" when there is no brace, which says so in hadBody. Returns the
	// tag, empty for an enum without one.
	std::string parseEnumDeclaration(bool& hadBody);

	// Records a file-scope name, or throws when it is already taken by something
	// the typedef and enum support has to keep distinct from it.
	void declareName(const std::string& name, const char* what);

	// Records a parameter or a local, and throws when it would hide a typedef or an
	// enum constant.
	void declareLocal(const std::string& name);

	// "[::] a [:: b ...]" at the current token, without consuming it.
	struct QualifiedName {
		std::vector<std::string> parts;
		bool global = false;
		size_t tokens = 0;
	};

	// A name as it is declared in the namespace the parser is in.
	std::string qualify(const std::string& name) const;
	std::string namespacePrefix(size_t depth) const;
	std::string namespaceOf(const QualifiedName& name, size_t count) const;

	void parseNamespace();
	void parseUsing();

	bool peekQualifiedName(QualifiedName& out) const;

	// The file-scope name a spelling refers to from here: its qualified name, or the
	// spelling itself when it names no declaration (a builtin). A qualified spelling
	// that names nothing is a CompileError.
	std::string resolveName(const QualifiedName& name) const;
	// The name at the current token, resolved, and how many tokens spell it.
	std::string peekResolved(size_t& tokens) const;
	void collectOwn(const std::string& scope, const std::string& name, std::set<std::string>& found) const;
	void collectMembers(const std::string& scope, const std::string& name, std::set<std::string>& seen,
		std::set<std::string>& found) const;
	bool isLocal(const std::string& name) const;

	// Opens the scope of a block, a for statement or a function's parameters.
	class LocalScope {
		Parser& _parser;

	public:
		explicit LocalScope(Parser& parser): _parser(parser) { _parser._locals.emplace_back(); }
		~LocalScope() { _parser._locals.pop_back(); }
		LocalScope(const LocalScope&) = delete;
		LocalScope& operator=(const LocalScope&) = delete;
	};

	// Resolves a scalar, vector, matrix or typedef name to its type.
	bool resolveTypeName(std::string_view text, Type& out) const;

	// A constant integer expression: literals, enum constants and the operators
	// on them. Anything that is not a constant is a CompileError.
	int64_t evaluateConstant(const Expression& expression, bool nested) const;
	int64_t parseConstantIndex(const std::string& context, bool isAttribute = false);
	FunctionDecl parseFunctionDeclaration(Stage stage);
	// A file-scope function that is not an entry point. False, with the position
	// restored, when the tokens ahead are not a function's return type, name and
	// opening parenthesis, so a file-scope constant still reaches its own parser.
	bool tryParseHelperFunction();
	size_t _functionOrder = 0;
	VariableDeclaration parseGlobalDeclaration();

	// Reads a bare identifier used as a name, such as a struct field's.
	void expectFieldName(std::string& out);

	// An initialiser, which is an expression or a braced list. A list is only
	// spelled in an initialiser, so it is not a primary expression.
	ExpressionPtr parseInitializer();
	ExpressionPtr parseInitializerList();

	// Types
	// allowResource is true where a texture or sampler type may be spelled: an
	// entry point parameter, and a local declaration, which only a sampler may be.
	Type parseType(bool allowResource = false);
	size_t parseSpecifierRun(Type& type);
	void parseResourceType(Type& type);
	bool parseQualifier(Type& type, bool afterPointer);
	void parseSamplerLocal(VariableDeclaration& declaration);
	SamplerState parseSamplerOptions(TokenKind closing);
	bool parseAddressSpace(AddressSpace& space);
	std::optional<uint32_t> tryParseArrayLength();

	// Parameters
	// helperName is set for a helper function's parameter, which has fewer forms
	// than an entry point's: no attribute, no resource, no pointer or reference.
	Parameter parseParameter(const std::string& helperName = std::string());
	ParameterAttributes parseParameterAttributes();

	// Struct fields
	FieldAttributes parseFieldAttributes();

	// One [[...]] attribute list, handing each attribute's name and its optional
	// constant integer argument to visit. Parameters and struct fields spell the
	// list the same way, so they share the scanner.
	void parseAttributeList(const std::function<void(const std::string&, std::optional<uint32_t>,
		const std::string&)>& visit);

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
	ExpressionPtr parseConditional();
	ExpressionPtr parseCast();
	ExpressionPtr parseNamedCast();

	std::optional<Type> peekCastTargetName(size_t& nameTokens);
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
			measure(*expression);

			left = std::move(expression);
		}
	}
};

}
