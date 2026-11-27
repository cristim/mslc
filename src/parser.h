#pragma once

#include "ast.h"

#include <cstdint>
#include <deque>
#include <exception>
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

// A template argument that makes a template's declaration ill-formed (an enable_if that is not
// satisfied). During overload resolution it removes that template from the candidates, as in C++
// (SFINAE); anywhere else it is an error like any other.
class SubstitutionFailure: public CompileError {
public:
	using CompileError::CompileError;
};

// One instance of a function template as an ordinary helper: the template's tokens re-parsed
// with the arguments bound, one per template parameter and in order. With withBody false only
// the signature is read, so that a call can be ranked before an instance is committed to.
// A CompileError says what in the template the subset does not lower.
FunctionDecl instantiateTemplate(const FunctionTemplate& declaration,
	const std::vector<TemplateArgument>& arguments, bool withBody);

// The default of template parameter "index", read under the arguments bound before it.
TemplateArgument templateDefault(const FunctionTemplate& declaration, size_t index,
	const std::vector<TemplateArgument>& bound);

// A function template's instance names an instance of a class template that the file has not made. The
// parser that reads the instance cannot declare it, so compile parses again with the seed made up front.
class NeedClassInstance: public std::exception {
public:
	explicit NeedClassInstance(ClassInstanceSeed seed): seed(std::move(seed)) {}
	const char* what() const noexcept override { return "a class template instance is needed"; }
	ClassInstanceSeed seed;
};

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
		std::string owner; // the tag of that enum, or "<anonymous>" when it has none
	};
	std::map<std::string, EnumConstant> _enumConstants;

	// A const or constexpr integer whose initialiser folded, so an array length may name it.
	// File-scope ones are kept by qualified name, local ones in the block that declares them.
	struct ConstantInt {
		int64_t value;
		ScalarKind kind;
	};
	std::map<std::string, ConstantInt> _constantInts;
	// A const or constexpr one-dimensional array of integers whose elements all folded, so a
	// constant expression may index it (an attribute index written xlt_remap[0]). Kept like
	// _constantInts: file-scope ones here, local ones in the block that declares them.
	struct ConstantIntArray {
		std::vector<int64_t> values;
		ScalarKind kind;
	};
	std::map<std::string, ConstantIntArray> _constantIntArrays;
	// The struct whose body is being parsed, so an array length can tell a member from a constant.
	const StructDecl* _structBody = nullptr;
	std::map<std::string, ScalarKind> _enumUnderlying; // tag or anonymous typedef name -> underlying type
	ScalarKind _lastEnumUnderlying = ScalarKind::Int;

	// Every file-scope name and what it names, so a typedef or an enum constant
	// that collides with another declaration is reported, as it is in Apple's
	// compiler, instead of the later one silently winning.
	std::map<std::string, std::string> _declared;
	// The second kind of a name declared as a tag and as a function or variable.
	std::map<std::string, std::string> _declaredBesideTag;

	// The namespaces the file declares. A declaration inside one is stored under
	// its qualified name, "A::B::x", in every table above, so a name lookup is a
	// string lookup and a clash is found by declareName as it is at file scope.
	// A scope is keyed by its qualified name, "" being the file; it holds the
	// namespaces a using directive named and the names a using declaration brought in.
	struct NamespaceScope {
		std::vector<std::string> directives;
		// The inline namespaces declared directly in this one: their members are this
		// scope's own for every lookup, not only through a directive.
		std::vector<std::string> inlines;
		// A name to what using declarations brought in under it: several functions
		// overload, so a call is resolved (or found ambiguous) where it is made.
		std::map<std::string, std::set<std::string>> declarations;
		// Standard library names a using declaration brought in, as "using metal::clamp;".
		std::set<std::string> stdlibDeclarations;
	};
	std::vector<std::string> _namespacePath;
	std::set<std::string> _namespaces;
	std::map<std::string, std::string> _namespaceAliases;
	std::map<std::string, NamespaceScope> _scopes;

	// The blocks of the function being parsed, innermost last. A name maps to "" for
	// a local, which hides a file-scope name of that spelling, or to what a using
	// declaration in the block names. A block's using directives are searched as if
	// written in the function's namespace, and only while the block is open.
	struct BlockScope {
		std::map<std::string, std::string> names;
		std::map<std::string, std::string> aliases;
		std::map<std::string, ConstantInt> constants;
		std::map<std::string, ConstantIntArray> constantArrays;
		// The declared type of each object whose type is final and sizeof can use; a name this block
		// declares without an entry here is one whose type is not known (auto, a resource).
		std::map<std::string, Type> objectTypes;
		std::vector<std::string> directives;
		// Set on the outermost block of a function body or a for body, whose names share a C++
		// scope with the enclosing parameters or init declaration.
		bool sharesEnclosingScope = false;
	};
	std::vector<BlockScope> _blocks;

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

	explicit Parser(std::vector<Token> tokens): _tokens(std::move(tokens)) { reserveSplits(_tokens); }

	TranslationUnit parse();

	// Class template instances to make before the first declaration that can see their class template and
	// argument types, in order (an argument may be an earlier seed).
	void setSeeds(std::vector<ClassInstanceSeed> seeds);

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
	StatementPtr parseBody(bool sharesEnclosingScope = false);

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

	// Everything a name lookup reads, copied where a function template is declared, so that
	// each instance of it is parsed as if at that point of the file.
	struct Names {
		std::map<std::string, Type> typedefs;
		std::map<std::string, std::string> enumTypes;
		std::map<std::string, EnumConstant> enumConstants;
		std::map<std::string, ScalarKind> enumUnderlying;
		std::map<std::string, std::string> declared;
		std::map<std::string, std::string> declaredBesideTag;
		std::vector<std::string> namespacePath;
		std::set<std::string> namespaces;
		std::map<std::string, std::string> namespaceAliases;
		std::map<std::string, NamespaceScope> scopes;
		std::deque<StructDecl> structs;
		std::set<std::string> functionTemplates;
		std::map<std::string, std::shared_ptr<const ClassTemplate>> classTemplates;
		std::map<std::string, ConstantInt> constantInts;
		std::map<std::string, ConstantIntArray> constantIntArrays;
		std::map<std::string, ConstructorInfo> constructors;
		std::set<std::string> memberTemplateNames;
		std::set<std::string> seedKeys;
	};
	std::shared_ptr<const Names> captureNames() const;
	void restoreNames(const Names& names);
	// The name state where a template's declaration ends, read from the file once and kept.
	static std::shared_ptr<const Names> namesAt(const FunctionTemplate& declaration);
	size_t _stopAt = SIZE_MAX;
	// An instance being read: the block that holds its parameters, and the parameters whose declared type
	// names a template parameter.
	size_t _parameterBlock = SIZE_MAX;
	std::set<std::string> _dependentParameters;
	bool namesDependentParameter(const std::string& name) const;
	friend FunctionDecl instantiateTemplate(const FunctionTemplate&, const std::vector<TemplateArgument>&, bool);
	friend TemplateArgument templateDefault(const FunctionTemplate&, size_t, const std::vector<TemplateArgument>&);
	FunctionDecl parseInstance(const FunctionTemplate& declaration, const std::vector<TemplateArgument>& arguments,
		bool withBody);
	// Binds the parameters of a template in an outermost block: a type as a typedef, a constant
	// as an enumerator-like constant, each under a key no source spelling can reach.
	void bindTemplateArguments(const FunctionTemplate& declaration, const std::vector<TemplateArgument>& arguments);
	void bindParameters(const std::string& owner, const std::vector<TemplateParameter>& parameters,
		const std::vector<TemplateArgument>& arguments);

	// Class templates, by qualified name. A use that spells "Name<arguments>" is read as one name, that of the
	// instance, which declares the struct the first time it is met (see instantiateClass).
	std::map<std::string, std::shared_ptr<const ClassTemplate>> _classTemplates;
	// Instances being read, so that a member naming its own instance finds it.
	std::set<std::string> _classInProgress;
	size_t _classDepth = 0;
	// This parser reads a function template's instance (parseInstance); each instance has its own parser.
	bool _inInstance = false;
	// Indices into _unit.helpers already stamped by a class instance, so an enclosing instance leaves them.
	std::set<size_t> _instanceHelpers;
	// A parser that reads a function template instance declares no struct: the unit it fills is not the one
	// the emitter reads, so an instance it needs has to exist already.
	bool _frozenClasses = false;
	// The seeds this parse was given, which are made once their class template and arguments exist.
	std::vector<ClassInstanceSeed> _seeds;
	std::vector<bool> _seeded;
	std::set<std::string> _seedKeys;
	std::shared_ptr<const std::vector<ClassInstanceSeed>> _seedList;
	void makeSeeds();
	void parseTopLevel() {
		makeSeeds();
		parseDeclaration();
	}
	// The spelling of an instance's name, which the tokens that carry it point into.
	std::shared_ptr<std::deque<std::string>> _spellings = std::make_shared<std::deque<std::string>>();
	// The parts of the parser's position that belong to what is being read, which an instance reads over
	// (swapContext), so that a name in one never sees a local of the place that named it.
	struct Context {
		std::vector<Token> tokens;
		size_t position = 0;
		std::vector<BlockScope> blocks;
		const StructDecl* structBody = nullptr;
		std::vector<size_t> pendingMembers;
		std::vector<FunctionTemplate> pendingMemberTemplates;
		const std::set<std::string>* memberFields = nullptr;
		std::map<std::string, bool> memberFunctions;
		std::string memberStruct;
		size_t parameterBlock = SIZE_MAX;
		std::set<std::string> dependentParameters;
		int switchDepth = 0;
		std::vector<StatementPtr> declaratorTail;
		bool noGreater = false;
		bool inEnableIf = false;
		bool dependentCondition = false;
		size_t stopAt = SIZE_MAX;
		std::vector<std::string> namespacePath;
	};
	void swapContext(Context& other);
	// Room for every ">>" to become two tokens, so that splitting one while a name is peeked never moves
	// the tokens a caller holds a reference to.
	static void reserveSplits(std::vector<Token>& tokens);
	// The using directives and declarations in the scopes a name in a template of this namespace path is read from.
	size_t usingCount(const std::vector<std::string>& path, const std::set<std::string>& mentioned,
		const std::vector<std::string>& aliasTargets) const;
	// A call in a class template that argument-dependent lookup could send to a function declared below it,
	// checked once the whole file is read (clang instantiates at the end of the file).
	struct AdlCheck {
		std::string templateName;
		size_t order = 0;
		std::string call;
	};
	size_t usingDeclarationsOf(const std::string& name) const;
	std::vector<AdlCheck> _adlChecks;
	void parseClassTemplate(ClassTemplate& declaration);
	// The class template a name spelled by parts names, or null; a local of the name hides it.
	const ClassTemplate* classTemplateNamed(const std::vector<std::string>& parts, bool global) const;
	std::vector<TemplateArgument> parseClassTemplateArguments(const ClassTemplate& declaration);
	// The qualified name of the instance, declared if it is new.
	std::string instantiateClass(const ClassTemplate& declaration, std::vector<TemplateArgument> arguments);
	TemplateArgument parseTemplateArgument();
	void parseTemplateHeader(FunctionTemplate& declaration);
	std::string readTemplateFunction(FunctionTemplate& declaration, size_t headerStart, bool member);
	// "template <...> R f(...) const {...}" in a struct body: kept as tokens until the struct is complete,
	// then declared as a function template whose first parameter is the object (see finishStructFunctions).
	void parseMemberTemplate(const StructDecl& decl);
	std::vector<FunctionTemplate> _pendingMemberTemplates;
	// The names of the member function templates declared so far, so that "v.f<float>(x)" reads its arguments.
	std::set<std::string> _memberTemplateNames;
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

	// [namespace.udecl]: a declaration and a using declaration of one name in one
	// namespace conflict, unless one is a struct or enum tag beside a function or
	// variable, or both are functions with different parameters. "function" is the
	// declaration being parsed when "name" is one, and null otherwise. wideTagExemption
	// widens the tag exemption to any other kind (a typedef or an enum constant too),
	// which only holds for a using declaration checked against a name already declared,
	// not the other order (parseUsing passes true; rejectUsingConflict passes false).
	bool conflictsWithUsing(const std::string& own, const std::vector<std::string_view>& ownKinds,
		const std::string& target, const FunctionDecl* function, bool wideTagExemption = false) const;
	// What a declared name is: one kind, or a struct or enum tag and a function or variable.
	std::vector<std::string_view> kindsOf(const std::string& name) const;
	void rejectUsingConflict(const std::string& name, std::string_view what, const FunctionDecl* function) const;

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
	enum class NamespaceLookup { Qualifier, NamespaceOnly };
	struct NamespaceTier {
		std::set<std::string> found;
		bool aliases = false;
	};
	std::vector<NamespaceTier> namespaceTiers(const QualifiedName& name, NamespaceLookup mode) const;
	std::string aliasQualifier(const QualifiedName& name, NamespaceLookup mode) const;
	std::string blockQualifier(const std::string& part, NamespaceLookup mode) const;
	std::string namespaceOf(const QualifiedName& name, size_t count, NamespaceLookup mode = NamespaceLookup::Qualifier) const;
	// The namespace a path names, or "" if it names none.
	std::string canonicalNamespace(const std::string& path) const;
	// The namespaces "part" names from "scope", by the walk namespaceOf's later parts use.
	std::set<std::string> namespacesNamed(const std::string& scope, const std::string& part, bool qualified,
		std::set<std::string>& seen, NamespaceLookup mode, bool* aliases = nullptr) const;
	// The one namespace "part" names inside "scope" ("" if none); several is ambiguous.
	std::string childNamespace(const std::string& scope, const std::string& part, bool qualified, NamespaceLookup mode) const;

	void parseNamespace(bool isInline = false);
	void parseNamespaceAlias();
	std::vector<std::string> inlineClosure(const std::string& scope) const;
	// Whether "path" is a namespace: one the file declares, or the library's metal.
	bool isNamespacePath(const std::string& path) const;
	// How many leading parts of the name ahead spell the library's namespace metal by an
	// alias (or by a leading "::"), and the tokens they take with the "::" after them; the
	// plain, nearest "metal::" is the literal branches' and counts as none.
	size_t libraryPrefixParts(const QualifiedName& name) const;
	size_t libraryPrefixTokens() const;
	// Whether the cursor is at metal::array<, spelled with or without the namespace unless the file
	// declares its own array; nameTokens is how many tokens the name takes.
	bool atStdArray(size_t& nameTokens) const;
	// array<T, N> after its qualifiers, folded into type as the array T[N] with isStdArray set.
	void parseStdArray(Type& type, size_t nameTokens);
	// Whether the cursor is at metal::as_type<, spelled with or without the namespace unless the file
	// declares its own as_type or a local hides it; nameTokens is how many tokens the name takes.
	bool atAsType(size_t& nameTokens) const;
	// as_type<T>(x), read as a call of the builtin __metal_as_type with T as its template argument.
	ExpressionPtr parseAsType(size_t nameTokens);
	// Whether a using directive in scope, directly or through the namespaces it nominates, reaches metal.
	bool libraryNominatedFromHere() const;
	// The library's resource type name that a library prefix qualifies, or 0.
	size_t libraryResourcePrefixTokens() const;
	void parseUsing();

	bool peekQualifiedName(QualifiedName& out) const;
	bool isDiscardFragment(QualifiedName& name) const;

	// The file-scope name a spelling refers to from here: its qualified name, or the
	// spelling itself when it names no declaration (a builtin). A qualified spelling
	// that names nothing is a CompileError.
	// With "alternatives", a name that finds several functions is their overload set, not an
	// ambiguity: they are returned there and the first is the result.
	std::string resolveName(const QualifiedName& name, std::vector<std::string>* alternatives = nullptr) const;
	std::string resolveNameUnchecked(const QualifiedName& name, std::vector<std::string>* alternatives) const;
	// While an instance of a class template is read: the names its template was declared above.
	const std::set<std::string>* _definedNames = nullptr;
	// The declarations an unqualified name finds at the innermost namespace level that has
	// any, without reporting several as ambiguous (a declaration site is not a use). With
	// "besideStdlib", also whether the standard library's names are visible there or nearer:
	// the level is namespace metal, or a using directive or declaration brings them to it.
	std::set<std::string> unqualifiedFound(const std::string& last, bool* besideStdlib = nullptr) const;
	// The name at the current token, resolved, and how many tokens spell it.
	std::string peekResolved(size_t& tokens, std::vector<std::string>* alternatives = nullptr) const;
	void collectOwn(const std::string& scope, const std::string& name, std::set<std::string>& found) const;
	// With "besideStdlib", also whether the walk visited namespace metal or a scope with a
	// "using metal::name;" of this name, directly or through a directive it follows.
	void collectMembers(const std::string& scope, const std::string& name, std::set<std::string>& seen,
		std::set<std::string>& found, bool qualified, bool* besideStdlib = nullptr) const;
	bool isLocal(const std::string& name) const;
	// What the innermost block that declares the name binds it to: "" for a local,
	// the declaration a using declaration names, or nullptr when no block does.
	const std::string* blockName(const std::string& name) const;

	// Opens the scope of a block, a for statement or a function's parameters.
	class LocalScope {
		Parser& _parser;

	public:
		explicit LocalScope(Parser& parser): _parser(parser) { _parser._blocks.emplace_back(); }
		~LocalScope() { _parser._blocks.pop_back(); }
		LocalScope(const LocalScope&) = delete;
		LocalScope& operator=(const LocalScope&) = delete;
	};

	// Resolves a scalar, vector, matrix or typedef name to its type.
	bool resolveTypeName(std::string_view text, Type& out) const;
	bool objectHidesTag(const std::string& name) const;
	bool memberHidesName(const std::string& name) const;

	// A constant integer expression: literals, enum constants and the operators
	// on them. Anything that is not a constant is a CompileError.
	int64_t evaluateConstant(const Expression& expression, bool nested, bool attribute = false) const;
	// The value of a named integer constant in scope, or a CompileError saying why it is not one.
	int64_t namedConstantValue(const Expression& identifier, bool nested, bool attribute) const;
	int64_t constantArrayElement(const Expression& index, bool nested, bool attribute) const;
	void recordConstantArray(const VariableDeclaration& declaration, bool local);
	// Remembers a const or constexpr integer declaration whose initialiser folds; local is the
	// current block's, otherwise the file's.
	void recordConstant(const VariableDeclaration& declaration, bool local);
	void recordObjectType(const std::string& name, const Type& type);
	std::optional<Type> declaredObjectType(const Expression& operand) const;
	// declaredObjectType, and a member of a declared struct value that is itself typed this way.
	std::optional<Type> declaredPartType(const Expression& operand) const;
	// The expression an array<T, N> member call is: a constant of the length for size(),
	// max_size() and empty(), the element at 0 or N - 1 for front() and back(). Null when the
	// receiver is not a declared array<T, N> or the call is another one; consumes the "()" if it
	// is one.
	ExpressionPtr foldStdArrayCall(ExpressionPtr& receiver);
	int64_t parseConstantIndex(const std::string& context, bool isAttribute = false);
	FunctionDecl parseFunctionDeclaration(Stage stage);
	// A file-scope function that is not an entry point. False, with the position
	// restored, when the tokens ahead are not a function's return type, name and
	// opening parenthesis, so a file-scope constant still reaches its own parser.
	bool tryParseHelperFunction();
	void parseTemplateDeclaration();
	void skipBalanced(TokenKind open, TokenKind close);
	void skipFunctionAttribute();
	// The <type_traits> subset (is_same, is_floating_point, enable_if, make_scalar ...), folded
	// where the types are known.
	void closeAngle(const char* what);
	std::string libraryName(const std::string& resolved) const;
	ExpressionPtr tryParseTraitValue();
	void parseTraitType(const std::string& name, Type& type);
	bool _noGreater = false;
	bool _inEnableIf = false;
	bool _dependentCondition = false;
	size_t _functionOrder = 0;
	void parseGlobalDeclaration();
	VariableDeclaration parseGlobalDeclarator(VariableDeclaration declaration);

	// Reads a bare identifier used as a name, such as a struct field's.
	void expectFieldName(std::string& out);

	// An initialiser, which is an expression or a braced list. A list is only
	// spelled in an initialiser, so it is not a primary expression.
	ExpressionPtr parseInitializer();
	ExpressionPtr parseInitializerList();

	// Types
	// allowResource is true where a texture or sampler type may be spelled: an
	// entry point parameter, and a local declaration, which only a sampler may be.
	// allowAuto is true only for a local, the one place an auto type is lowered.
	// base, when given, receives the type before any star, for the declarators that follow the first.
	Type parseType(bool allowResource = false, bool allowAuto = false, Type* base = nullptr);
	void parseDeclaratorStars(Type& type);
	size_t parseSpecifierRun(Type& type);
	void parseResourceType(Type& type);
	bool parseQualifier(Type& type, bool afterPointer);
	VariableDeclaration parseNextLocalDeclarator(const Type& base);
	void parseSamplerLocal(VariableDeclaration& declaration);
	bool parensDeclareFunction();
	void rejectParameterListInitializer(const VariableDeclaration& declaration);
	void parseLocalInitializer(VariableDeclaration& declaration);
	void parseAutoInitializer(VariableDeclaration& declaration);
	void appendStatement(std::vector<StatementPtr>& out);
	SamplerState parseSamplerOptions(TokenKind closing);
	bool parseAddressSpace(AddressSpace& space);
	std::optional<uint32_t> tryParseArrayLength();
	// "[N]", "[N][M]" or "[]" after a declarator's name, set on the type; true for "[]".
	bool parseArrayDimensions(Type& type);
	// The omitted outermost length of "T a[] = {...}", taken from the list.
	void deduceArrayLength(VariableDeclaration& declaration);

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
	// Switch bodies being parsed, which is where [[fallthrough]] is allowed.
	int _switchDepth = 0;
	// The declarations after the first of "T a, b, c;", which parseStatement leaves
	// for the list it is appending to.
	std::vector<StatementPtr> _declaratorTail;
	StatementPtr parseCompoundStatement(bool sharesEnclosingScope = false);
	StatementPtr parseIfStatement();
	StatementPtr parseForStatement();
	StatementPtr parseWhileStatement();
	StatementPtr parseDoWhileStatement();
	StatementPtr parseSwitchStatement();
	StatementPtr parseReturnStatement();

	// Expressions, by precedence level.
	ExpressionPtr parseExpression();

	// "<float, 3>" after the name of a member function template in a call, read into member's template arguments.
	void parseMemberTemplateArguments(Expression& member, bool keyword = false);
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
	ExpressionPtr parseSizeof();
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
