#pragma once

#include "lexer.h"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace mslc {

// Metal storage classes that can qualify a parameter. Each maps onto a
// different SPIR-V storage class, so the distinction is load-bearing rather
// than decorative.
enum class AddressSpace {
	None,
	Device,
	Constant,
	Threadgroup,
	Thread,
};

enum class Stage {
	None,
	Vertex,
	Fragment,
	Kernel,
};

// Scalar type kinds. Width is in bits; vectors are a count of one of these.
enum class ScalarKind {
	Void,
	Bool,
	Char,
	UChar,
	Short,
	UShort,
	Int,
	UInt,
	Long,
	ULong,
	Half,
	Float,
	Double,
};

// The texture and sampler types an entry point can take. A texture's component
// type, the T of texture2d<T> and the others, is in Type::scalar.
enum class ResourceKind {
	None,
	Texture1D,
	Texture2D,
	TextureCube,
	Texture3D,
	Texture2DArray,
	Texture1DArray,
	TextureCubeArray,
	// texture_buffer<T, access>: a one-dimensional texel buffer, read by integer index, with no
	// mip levels, no sampler and no sampling.
	TextureBuffer,
	Sampler,
};

enum class TextureAccess { Sample, Write, Read, ReadWrite };

// The MSL spelling of an access qualifier, such as "read_write".
const char* textureAccessName(TextureAccess access);

struct Type {
	// Scalar base. For a named type, a pointer, or an array, the base is the
	// element type and the wrappers below describe the rest.
	ScalarKind scalar = ScalarKind::Void;

	// 0 for a scalar, 2/3/4/8/16 for a vector. For a matrix, the row count, which
	// is the width of each column vector.
	uint32_t vectorWidth = 0;

	// 0 unless this is a matrix, which MSL spells floatCxR: C columns of R rows.
	uint32_t matrixColumns = 0;

	// Set for packed_float3 and the other packed_<scalar><2-4> vectors. Its value
	// is the same vector as the unpacked spelling's; only the storage layout
	// differs: the components sit back to back and the alignment is the
	// component's, where float3 takes 16 bytes aligned to 16.
	bool isPacked = false;

	// Set for a type referred to by name, such as a struct. Resolved during
	// semantic analysis.
	std::string namedType;

	// Set for atomic_uint and atomic_int: the scalar is the integer they hold, which is
	// only read and written by the atomic_*_explicit functions.
	bool isAtomic = false;

	// Set for an unscoped enum, to its tag: the type is stored as its underlying
	// integer, but Apple does not convert an integer or another enum to it.
	std::string enumName;

	bool isPointer = false;
	bool isConst = false;
	// Written volatile. Nothing but the type traits and template arguments reads it (it changes no
	// code), and they refuse it, since is_same<volatile int, int> is false.
	bool isVolatile = false;

	// Written as auto: the scalar, vector and name above are not known until
	// semantic analysis deduces them from the initialiser. Only a local has one.
	bool isAuto = false;

	// As written. Only a local or a file-scope constant can be constexpr, and
	// nothing inside a function can be static; the parser reports both where
	// the context is known.
	bool isConstexpr = false;
	bool isStatic = false;
	AddressSpace addressSpace = AddressSpace::None;

	// Set for an array type; the element type is described by the rest.
	std::optional<uint32_t> arrayLength;
	// Spelled metal::array<T, N> rather than T[N]: the same layout, but a value that a copy, an
	// assignment, a by-value parameter and a return carry. Meaningful only with arrayLength set.
	bool isStdArray = false;
	// The lengths inside the outermost one, outer to inner: float a[2][3] is
	// arrayLength 2 and innerArrayLengths {3}.
	std::vector<uint32_t> innerArrayLengths;

	// The element of an array type: the type with its outermost dimension taken off.
	void dropOuterDimension() {
		if (innerArrayLengths.empty()) {
			arrayLength.reset();
			isStdArray = false;
			return;
		}
		arrayLength = innerArrayLengths.front();
		innerArrayLengths.erase(innerArrayLengths.begin());
	}
	size_t arrayRank() const { return arrayLength ? 1 + innerArrayLengths.size() : 0; }

	ResourceKind resource = ResourceKind::None;
	TextureAccess textureAccess = TextureAccess::Sample;
	// A depth2d, depth2d_array, depthcube or depthcube_array: the texture kind above with a
	// depth image, whose lookups return one float instead of a four-channel texel.
	bool isDepth = false;
	// A texture2d_ms, texture2d_ms_array, depth2d_ms or depth2d_ms_array: read by texel and sample
	// index with no sampler and no mip levels.
	bool isMultisampled = false;

	bool isScalar() const { return vectorWidth == 0; }
	bool isVector() const { return vectorWidth > 1; }
	bool isMatrix() const { return matrixColumns > 0; }
	bool isTexture() const {
		return resource == ResourceKind::Texture1D || resource == ResourceKind::Texture2D
			|| resource == ResourceKind::TextureCube || resource == ResourceKind::Texture3D || resource == ResourceKind::Texture2DArray
			|| resource == ResourceKind::Texture1DArray || resource == ResourceKind::TextureCubeArray
			|| resource == ResourceKind::TextureBuffer;
	}
	// A texture lowered to a storage image. A texture_buffer that is only read is a sampled
	// texel buffer instead, fetched by index, so that its descriptor is a uniform texel buffer.
	bool isStorageTexture() const {
		return textureAccess != TextureAccess::Sample
			&& !(resource == ResourceKind::TextureBuffer && textureAccess == TextureAccess::Read);
	}
};

// One argument of a template: a type, or an integer constant (a bool is 0 or 1).
struct TemplateArgument {
	bool isType = true;
	Type type;
	int64_t value = 0;
	// A constant that is not known, as when a template's signature is read to deduce the rest:
	// its name is bound, and using it in the signature is an error.
	bool unbound = false;
};

// The MSL spelling of a resource kind, such as "texture2d".
const char* resourceKindName(ResourceKind kind);
const char* scalarKindName(ScalarKind kind);

// The sampled type of the SPIR-V image behind a texture of this element type: half is
// a float image, short an int one and ushort a uint one, narrowed after the read.
ScalarKind textureSampledKind(ScalarKind element);
uint32_t scalarBitWidth(ScalarKind kind);

// The type as it is written in the source: "float", "float3", or the name a
// struct was declared with. Diagnostics quote a type the way the reader wrote
// it, so this is deliberately the MSL spelling and not a SPIR-V one.
std::string typeName(const Type& type);

// The keyword an address space is written with, for diagnostics.
const char* addressSpaceName(AddressSpace space);

// Parameter attributes. MSL attaches these with [[...]] and they carry all the
// binding information, so they are modelled explicitly rather than discarded.
struct ParameterAttributes {
	// [[buffer(n)]]
	std::optional<uint32_t> bufferIndex;

	// [[threadgroup(n)]]: a kernel's threadgroup memory argument
	std::optional<uint32_t> threadgroupIndex;

	// [[texture(n)]] and [[sampler(n)]]
	std::optional<uint32_t> textureIndex;
	std::optional<uint32_t> samplerIndex;

	// [[thread_position_in_grid]] and friends. Only the ones the subset
	// supports are named; anything else is a hard error at parse time so an
	// unsupported builtin cannot be silently ignored.
	enum class Builtin {
		None,
		ThreadPositionInGrid,
		ThreadgroupPositionInGrid,
		ThreadPositionInThreadgroup,
		ThreadIndexInThreadgroup,
		ThreadsPerThreadgroup,
		VertexID,
		InstanceID,
		Position,
		FragCoord,
		FrontFacing,
	};
	std::optional<Builtin> builtin;

	// [[stage_in]]
	bool stageIn = false;

	// [[color(n)]]: framebuffer fetch of colour attachment n
	std::optional<uint32_t> colorIndex;
};

struct Parameter {
	Type type;
	std::string name;
	ParameterAttributes attributes;
	// Declared with "&". On an entry point only a [[stage_in]] parameter reads
	// it; a helper's non-const reference is the caller's variable.
	bool isReference = false;

	// A helper parameter that names the caller's variable, as opposed to a copy
	// of it. A reference to const cannot change it, so it is passed as a copy.
	bool isMutableReference() const {
		return isReference && !type.isConst && type.resource == ResourceKind::None;
	}
};

struct Expression;
using ExpressionPtr = std::unique_ptr<Expression>;

// One element of a braced initialiser list. Metal writes a struct's fields by
// name, as in ".direction = { 0.13, 0.72, 0.68 }", so the name is part of the
// element rather than a separate statement.
struct InitializerElement {
	std::string fieldName;
	ExpressionPtr value;
};

enum class ExpressionKind {
	IntLiteral,
	FloatLiteral,
	BoolLiteral,
	Identifier,
	Binary,
	Unary,
	Assign,
	Index,
	Member,
	Call,
	Construct,
	InitList,
	Conditional,
	// left, right: left is evaluated and dropped, the value is right's.
	Comma,
	// left: the operand of a sizeof whose size sema works out, since it needs the operand's type.
	// sizeof(type) never gets here: the parser folds it to a ulong IntLiteral.
	Sizeof,
};

enum class BinaryOperator {
	Add, Subtract, Multiply, Divide, Modulo,
	Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual,
	LogicalAnd, LogicalOr,
	BitAnd, BitOr, BitXor, ShiftLeft, ShiftRight,
};

enum class UnaryOperator {
	Negate, Plus, Not, BitNot, PreIncrement, PreDecrement, PostIncrement, PostDecrement,
	// &x[i]: the address of an element, which only an atomic function takes.
	AddressOf,
};

struct Expression {
	ExpressionKind kind;

	// literals
	uint64_t intValue = 0;
	// The type the literal has: int, uint, long or ulong, by C's rules for its
	// spelling and value. A long that did not fit holds the wrapped bits.
	ScalarKind intKind = ScalarKind::Int;
	double floatValue = 0.0;
	// An enumerator: the tag of the enum it belongs to, on the literal it became.
	std::string enumName;
	// Written with an `h` suffix: the literal is a half, not a float.
	bool floatIsHalf = false;
	bool boolValue = false;
	// IntLiteral: a ulong that sizeof gave, so a constant expression knows its value is
	// unsigned and does not let it wrap as a signed one would.
	bool isSizeof = false;

	// Identifier, Member, Index, Call
	std::string name;
	std::string memberName;
	std::vector<ExpressionPtr> arguments;

	// Index: the expression was written "*p", which is "p[0]" and is built as
	// that, so the two lower identically. Kept so a diagnostic can say which
	// spelling it is about.
	bool isDereference = false;

	// Member: written "p->m" rather than "v.m".
	bool isArrow = false;

	// Identifier: a name found where the standard library's names are also visible, so a
	// call is resolved against both.
	bool besideStdlib = false;

	// Identifier: the functions an unqualified lookup found in more than one scope (using
	// directives and declarations), which together are the overload set of a call. Empty when
	// the lookup found one name, which is then "name".
	std::vector<std::string> overloadNames;

	// Identifier: written without a qualifier and followed by "(", so argument-dependent lookup
	// adds the functions of its arguments' namespaces.
	bool unqualifiedCall = false;

	// Identifier: written "::name" while a local of that name is in scope, so the name
	// is the file-scope declaration and the local does not hide it.
	bool fileScope = false;

	// Identifier: the explicit template arguments of "f<float, 3>", as written, when it has any.
	bool hasTemplateArguments = false;
	std::vector<TemplateArgument> templateArguments;

	// Identifier: in a function template's instance, names a parameter (not a local of the same name) whose
	// declared type names a template parameter, so what it is stands or falls with the arguments.
	bool templateDependent = false;

	// Member: spelled "x.template f(...)" or "x.template f<...>(...)"; the member must then be a template.
	bool templateKeyword = false;

	// InitList, Call: a braced temporary or constructor call of a struct the source names (not through a
	// template parameter), whose type is that struct whatever its arguments are.
	bool namesConcreteStruct = false;

	// InitList
	std::vector<InitializerElement> elements;

	// Conditional: left is the condition, arguments[0] the value when it holds and
	// arguments[1] the value when it does not.

	// Binary, Unary, Assign
	BinaryOperator binaryOperator = BinaryOperator::Add;
	// Assign: set for "op=", where it names the operator, so "x <<= 3" is
	// "x = x << 3" with x evaluated once.
	std::optional<BinaryOperator> compoundOperator;
	UnaryOperator unaryOperator = UnaryOperator::Negate;
	ExpressionPtr left;
	ExpressionPtr right;

	// Construct: the type being constructed
	std::optional<Type> constructType;

	// Line the expression started on, for diagnostics.
	size_t line = 0;

	// Nodes on the longest path from here down to a leaf, this one included.
	// The parser sets it on every node that has a child, and refuses a tree
	// taller than it can walk without running out of stack.
	uint32_t height = 1;
};

enum class StatementKind {
	Compound,
	ExpressionStatement,
	DeclarationStatement,
	If,
	For,
	While,
	Switch,
	Return,
	Break,
	Continue,
	Discard,
};

struct Statement;
using StatementPtr = std::unique_ptr<Statement>;

// One label of a switch and the statements under it, up to the next label or
// the end of the body. A default label has no value; the value of a case label
// is the constant expression the selector is compared with.
struct SwitchCase {
	ExpressionPtr value;
	std::vector<StatementPtr> body;
	size_t line = 0;
};

enum class SamplerAddress { ClampToZero, ClampToEdge, Repeat, MirroredRepeat };
enum class SamplerFilter { Nearest, Linear };
enum class SamplerMipFilter { None, Nearest, Linear };
enum class SamplerBorder { TransparentBlack, OpaqueBlack, OpaqueWhite };
enum class SamplerCompare { Never, Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual, Always };

// The state of a sampler declared in the shader, with the defaults of a
// sampler declared with no options. Compare/anisotropy/LOD/border options are
// rejected by the parser, so none of them is modelled.
struct SamplerState {
	SamplerAddress sAddress = SamplerAddress::ClampToEdge;
	SamplerAddress tAddress = SamplerAddress::ClampToEdge;
	SamplerAddress rAddress = SamplerAddress::ClampToEdge;
	SamplerFilter magFilter = SamplerFilter::Nearest;
	SamplerFilter minFilter = SamplerFilter::Nearest;
	SamplerMipFilter mipFilter = SamplerMipFilter::None;
	SamplerCompare compare = SamplerCompare::Never;
	SamplerBorder border = SamplerBorder::TransparentBlack;
	uint32_t anisotropy = 1;
	// The lod_clamp range, with the largest half as the upper bound of a sampler that sets none.
	float lodMin = 0.0f;
	float lodMax = 65504.0f;
	bool normalizedCoordinates = true;

	bool operator==(const SamplerState& other) const {
		return sAddress == other.sAddress && tAddress == other.tAddress && rAddress == other.rAddress
			&& magFilter == other.magFilter && minFilter == other.minFilter
			&& mipFilter == other.mipFilter && compare == other.compare && border == other.border
			&& anisotropy == other.anisotropy && lodMin == other.lodMin && lodMax == other.lodMax && normalizedCoordinates == other.normalizedCoordinates;
	}
};

const char* samplerAddressName(SamplerAddress mode);
const char* samplerFilterName(SamplerFilter filter);
const char* samplerMipFilterName(SamplerMipFilter filter);
const char* samplerCompareName(SamplerCompare compare);
const char* samplerBorderName(SamplerBorder border);

struct VariableDeclaration {
	Type type;
	std::string name;
	ExpressionPtr initializer;

	// T& name = lvalue; : a name for the place the initialiser denotes.
	bool isReference = false;

	// A sampler local: the state its options spell.
	std::optional<SamplerState> sampler;
};

struct Statement {
	StatementKind kind;

	// ExpressionStatement with no expression: [[fallthrough]];
	bool isFallthrough = false;

	// Compound
	std::vector<StatementPtr> children;

	// ExpressionStatement, Return, If condition, For condition/increment
	ExpressionPtr expression;

	// DeclarationStatement
	std::optional<VariableDeclaration> declaration;

	// If
	StatementPtr thenBranch;
	StatementPtr elseBranch;

	// For
	std::optional<VariableDeclaration> forInitializer;
	ExpressionPtr forCondition;
	ExpressionPtr forIncrement;
	StatementPtr forBody;

	// While, and do-while when isDoWhile is set: the condition runs after the body
	ExpressionPtr whileCondition;
	StatementPtr whileBody;
	bool isDoWhile = false;

	// Switch: the selector is `expression`. Statements before the first label
	// are unreachable, as C++ says, and the preamble is where they are kept.
	std::vector<SwitchCase> switchCases;
	std::vector<StatementPtr> switchPreamble;
	// The last case of a nested switch ends in [[fallthrough]];, which falls out of the switch.
	bool switchFallsOut = false;

	size_t line = 0;
};

// A struct field's attribute list. Metal attaches these to the fields of a
// struct that crosses a stage boundary, and each one says where the field lands
// in the interface, so they are kept rather than discarded.
enum class Interpolation {
	None,
	Flat,
	CenterPerspective,
	CenterNoPerspective,
	CentroidPerspective,
	CentroidNoPerspective,
	SamplePerspective,
	SampleNoPerspective,
};

enum class DepthMode { Any, Less, Greater };

struct FieldAttributes {
	// [[position]]: the field is the stage's own position. On a vertex output
	// that is the BuiltIn Position; on a fragment input it is the FragCoord.
	bool position = false;

	// [[attribute(n)]]: the field is a vertex buffer input or a user output at
	// location n.
	std::optional<uint32_t> attributeIndex;

	// [[flat]], [[center_no_perspective]] and the rest: how a stage-crossing
	// field is interpolated. None when the field names no qualifier.
	Interpolation interpolation = Interpolation::None;

	// [[color(n)]]: the field is colour attachment n of a fragment output.
	std::optional<uint32_t> colorIndex;
	std::optional<DepthMode> depthMode;
	bool sampleMask = false;

	// [[point_size]]: the field is the vertex output's BuiltIn PointSize.
	bool pointSize = false;

	// [[user(name)]]: the name Apple pairs a vertex output with a fragment input
	// by, in place of the field's own name.
	std::optional<std::string> userName;
};

struct StructField {
	Type type;
	std::string name;
	FieldAttributes attributes;
};

struct StructDecl {
	bool hasConstructors = false;
	std::string name;
	std::vector<StructField> fields;
	// The class template this is an instance of and the arguments it was made with; empty for a plain
	// struct. A function template's parameter "Box<T>" deduces T from these.
	std::string classTemplate;
	std::vector<TemplateArgument> classArguments;
};

// A template parameter: a type, or an integer constant of the given type.
struct TemplateParameter {
	std::string name;
	bool isType = true;
	ScalarKind kind = ScalarKind::Int; // the type of a non-type parameter
	// The tokens after "=" (ending in an end-of-file token), or none when it has no default.
	bool hasDefault = false;
	std::vector<Token> defaultTokens;
};

struct ClassTemplate;

// An instance of a class template that a function template's body names and no earlier code did. A
// function template's instance is read by a parser that cannot declare a struct, so it asks for the
// instance (NeedClassInstance); the file is then parsed again with the instance made where its class
// template and argument types are first all declared.
struct ClassInstanceSeed {
	std::string className;
	std::vector<TemplateArgument> arguments;
	// The instance's struct name, as a use spells it.
	std::string instance;
};

// A function template, kept as tokens and the parser's name state where it was declared, so
// that each instantiation re-parses it with its arguments bound (see instantiateTemplate).
struct FunctionTemplate {
	// Qualified, as a call names it.
	std::string name;
	std::vector<TemplateParameter> parameters;
	// The declaration after the "template<...>" header, to the end of its body or ";", and an
	// end-of-file token.
	std::vector<Token> tokens;
	bool hasBody = false;
	// The header and declarator as one string: a prototype and its definition spell it alike.
	std::string signature;
	size_t line = 0;
	// Where the template is first declared, which is where a call can first use it, and where its
	// body is: they differ when a prototype comes first. The instances take the second, and
	// globalsBefore, as FunctionDecl does.
	size_t order = 0;
	size_t bodyOrder = 0;
	size_t globalsBefore = 0;
	// The parser's name state where the declaration ends is rebuilt, the first time an instance needs it,
	// by reading the file again up to endAt (copying it for every template is quadratic in the file).
	std::shared_ptr<const std::vector<Token>> file;
	size_t endAt = 0;
	mutable std::shared_ptr<const void> names;
	// The instances the file was parsed with, which the name state rebuilt at endAt has to make too.
	std::shared_ptr<const std::vector<ClassInstanceSeed>> seeds;

	// A member function template of a struct: its name (the struct's), the fields and member functions
	// the struct has (a bare name in the body means this.field or this.f(...)), and, for a member of a
	// class template's instance, that template and the arguments its parameters stand for. The name
	// state it is read in is the struct's end, kept in names.
	std::string memberOf;
	// A static member template has no object: it is called as S::f<...>(...) and its body reads no this.
	bool staticMember = false;
	std::set<std::string> memberFields;
	std::map<std::string, bool> memberFunctions;
	std::shared_ptr<const ClassTemplate> ownerTemplate;
	std::vector<TemplateArgument> ownerArguments;
	// Holds the spelling the tokens of an instance's name point into.
	std::shared_ptr<const void> spellings;
};

// A class template: "template <...> struct Name { ... };" kept as tokens, from the "struct" keyword to the
// ";", and an end-of-file token. A use that names it with arguments has the tokens re-parsed in place with the
// arguments bound, which declares an ordinary struct under the instance's name ("Name<float,4>").
struct ClassTemplate {
	// Qualified, and the name as written.
	std::string name;
	std::string spelled;
	std::vector<std::string> namespacePath;
	std::vector<TemplateParameter> parameters;
	std::vector<Token> tokens;
	size_t line = 0;
	// What the member functions of every instance count as declared above: where the template is, since
	// a function written after it is not one a name in its body can find at the definition.
	size_t order = 0;
	size_t globalsBefore = 0;
	// The file-scope names declared above the template: a name its instances resolve to that is not here
	// was declared after it, which clang does not find from the definition.
	std::set<std::string> declaredBefore;
	// How many using directives and declarations the template's namespaces hold where it is declared.
	size_t usingsBefore = 0;
	std::vector<std::string> aliasTargets;
	// Every identifier its tokens and defaults spell: a using declaration of any other name cannot change a read.
	std::set<std::string> mentioned;
	// How many using declarations of each such name exist, in every scope, where the template is declared.
	std::map<std::string, size_t> usingDeclarationsBefore;
};

// An instance of a class template over the type parameters of a function template ("Box<T>"), as the
// function's signature names it while those parameters are still to be deduced: the type is spelled
// "<Box<<T>>>" (a marker, as a bare "<T>" is), and this says what it stands for. A type argument is
// a marker itself, a concrete type, or another such instance.
struct DependentInstance {
	std::string classTemplate;
	std::vector<TemplateArgument> arguments;
};

struct FunctionDecl {
	Stage stage = Stage::None;
	Type returnType;
	std::string name;
	std::vector<Parameter> parameters;
	// Null for a prototype, which declares a helper function and defines nothing.
	StatementPtr body;
	size_t line = 0;

	// How many of the unit's globals the source declares above this function: the
	// ones its body can name.
	size_t globalsBefore = 0;

	// Where the declaration sits among the file's function declarations, entry
	// points and helpers alike, since a function can only be called from below its
	// first declaration.
	size_t order = 0;

	// The class template instances over a template parameter that the signature names, by the marker
	// spelling of their type; only a signature read to deduce has any.
	std::map<std::string, DependentInstance> dependentInstances;

	// Declared static, inline or constexpr (implicitly inline), or a member defined in its struct: internal to the
	// translation unit, so a Linkage object keeps it local instead of exporting it.
	bool isLocal = false;

	// A function that is not an entry point.
	bool isHelper() const { return stage == Stage::None; }
};

// Two declarations of one function name take the same parameters: a redeclaration,
// not an overload.
bool sameParameterTypes(const FunctionDecl& a, const FunctionDecl& b);

// The parameter list spelled canonically, as "float3,thread const float&": declarations
// that sameParameterTypes calls the same spell it the same, whatever their spelling.
std::string parameterSignature(const Parameter& parameter);
std::string parameterSignature(const FunctionDecl& function);

struct TranslationUnit {
	// A deque: a class template instance is added while a struct above it is still being read.
	std::deque<StructDecl> structs;
	std::vector<FunctionDecl> functions;

	// The file's helper functions, prototypes and definitions, in source order.
	std::vector<FunctionDecl> helpers;

	// Qualified names of the function templates the file declares.
	std::set<std::string> functionTemplates;
	// The qualified names of the inline namespaces, which argument-dependent lookup follows outward and inward.
	std::set<std::string> inlineNamespaces;

	// The function templates themselves, in declaration order. A template is lowered only where
	// a call instantiates it: instantiateTemplate re-parses its tokens with the arguments bound.
	std::vector<FunctionTemplate> templates;

	// File-scope declarations, in the order the source declares them. A
	// "constant" refers to the ones before it, so the order is the meaning.
	std::vector<VariableDeclaration> globals;

	// MSL entry points, keyed by function name.
	const FunctionDecl* findFunction(const std::string& name) const;
	const StructDecl* findStruct(const std::string& name) const;
};

}
