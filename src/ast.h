#pragma once

#include "lexer.h"

#include <memory>
#include <optional>
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
// type, the T of texture2d<T> or texturecube<T>, is in Type::scalar.
enum class ResourceKind {
	None,
	Texture2D,
	TextureCube,
	Sampler,
};

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

	bool isPointer = false;
	bool isConst = false;

	// As written. Only a local or a file-scope constant can be constexpr, and
	// nothing inside a function can be static; the parser reports both where
	// the context is known.
	bool isConstexpr = false;
	bool isStatic = false;
	AddressSpace addressSpace = AddressSpace::None;

	// Set for an array type; the element type is described by the rest.
	std::optional<uint32_t> arrayLength;

	ResourceKind resource = ResourceKind::None;

	bool isScalar() const { return vectorWidth == 0; }
	bool isVector() const { return vectorWidth > 1; }
	bool isMatrix() const { return matrixColumns > 0; }
	bool isTexture() const {
		return resource == ResourceKind::Texture2D || resource == ResourceKind::TextureCube;
	}
};

const char* scalarKindName(ScalarKind kind);
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
		VertexID,
		InstanceID,
		Position,
		FragCoord,
		FrontFacing,
	};
	std::optional<Builtin> builtin;

	// [[stage_in]]
	bool stageIn = false;
};

struct Parameter {
	Type type;
	std::string name;
	ParameterAttributes attributes;
	// Declared with "&". Only a [[stage_in]] parameter reads it.
	bool isReference = false;
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
};

enum class BinaryOperator {
	Add, Subtract, Multiply, Divide, Modulo,
	Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual,
	LogicalAnd, LogicalOr,
	BitAnd, BitOr, BitXor, ShiftLeft, ShiftRight,
};

enum class UnaryOperator {
	Negate, Plus, Not, BitNot, PreIncrement, PreDecrement, PostIncrement, PostDecrement,
};

struct Expression {
	ExpressionKind kind;

	// literals
	uint64_t intValue = 0;
	// The type the literal has: int, uint, long or ulong, by C's rules for its
	// spelling and value. A long that did not fit holds the wrapped bits.
	ScalarKind intKind = ScalarKind::Int;
	double floatValue = 0.0;
	// Written with an `h` suffix: the literal is a half, not a float.
	bool floatIsHalf = false;
	bool boolValue = false;

	// Identifier, Member, Index, Call
	std::string name;
	std::string memberName;
	std::vector<ExpressionPtr> arguments;

	// Index: the expression was written "*p", which is "p[0]" and is built as
	// that, so the two lower identically. Kept so a diagnostic can say which
	// spelling it is about.
	bool isDereference = false;

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
	Return,
	Break,
	Continue,
	Discard,
};

struct Statement;
using StatementPtr = std::unique_ptr<Statement>;

enum class SamplerAddress { ClampToZero, ClampToEdge, Repeat, MirroredRepeat };
enum class SamplerFilter { Nearest, Linear };
enum class SamplerMipFilter { None, Nearest, Linear };

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
	bool normalizedCoordinates = true;

	bool operator==(const SamplerState& other) const {
		return sAddress == other.sAddress && tAddress == other.tAddress && rAddress == other.rAddress
			&& magFilter == other.magFilter && minFilter == other.minFilter
			&& mipFilter == other.mipFilter && normalizedCoordinates == other.normalizedCoordinates;
	}
};

const char* samplerAddressName(SamplerAddress mode);
const char* samplerFilterName(SamplerFilter filter);
const char* samplerMipFilterName(SamplerMipFilter filter);

struct VariableDeclaration {
	Type type;
	std::string name;
	ExpressionPtr initializer;

	// A sampler local: the state its options spell.
	std::optional<SamplerState> sampler;
};

struct Statement {
	StatementKind kind;

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

	// While
	ExpressionPtr whileCondition;
	StatementPtr whileBody;

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

	// A function that is not an entry point.
	bool isHelper() const { return stage == Stage::None; }
};

struct TranslationUnit {
	std::vector<StructDecl> structs;
	std::vector<FunctionDecl> functions;

	// The file's helper functions, prototypes and definitions, in source order.
	std::vector<FunctionDecl> helpers;

	// File-scope declarations, in the order the source declares them. A
	// "constant" refers to the ones before it, so the order is the meaning.
	std::vector<VariableDeclaration> globals;

	// MSL entry points, keyed by function name.
	const FunctionDecl* findFunction(const std::string& name) const;
	const StructDecl* findStruct(const std::string& name) const;
};

}
