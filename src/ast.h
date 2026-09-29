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

struct Type {
	// Scalar base. For a named type, a pointer, or an array, the base is the
	// element type and the wrappers below describe the rest.
	ScalarKind scalar = ScalarKind::Void;

	// 0 for a scalar, 2/3/4/8/16 for a vector.
	uint32_t vectorWidth = 0;

	// Set for a type referred to by name, such as a struct. Resolved during
	// semantic analysis.
	std::string namedType;

	bool isPointer = false;
	bool isConst = false;
	AddressSpace addressSpace = AddressSpace::None;

	// Set for an array type; the element type is described by the rest.
	std::optional<uint32_t> arrayLength;

	bool isScalar() const { return vectorWidth == 0; }
	bool isVector() const { return vectorWidth > 1; }
};

const char* scalarKindName(ScalarKind kind);
uint32_t scalarBitWidth(ScalarKind kind);

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
	bool isConstReference = false;
};

struct Expression;
using ExpressionPtr = std::unique_ptr<Expression>;

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
	Cast,
};

enum class BinaryOperator {
	Add, Subtract, Multiply, Divide, Modulo,
	Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual,
	LogicalAnd, LogicalOr,
	BitAnd, BitOr, BitXor, ShiftLeft, ShiftRight,
};

enum class UnaryOperator {
	Negate, Plus, Not, BitNot, PreIncrement, PreDecrement,
};

struct Expression {
	ExpressionKind kind;

	// literals
	uint64_t intValue = 0;
	double floatValue = 0.0;
	bool boolValue = false;

	// Identifier, Member, Index, Call
	std::string name;
	std::string memberName;
	std::vector<ExpressionPtr> arguments;

	// Binary, Unary, Assign
	BinaryOperator binaryOperator = BinaryOperator::Add;
	UnaryOperator unaryOperator = UnaryOperator::Negate;
	ExpressionPtr left;
	ExpressionPtr right;

	// Cast target
	std::optional<Type> castType;

	// Line the expression started on, for diagnostics.
	size_t line = 0;
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

struct VariableDeclaration {
	Type type;
	std::string name;
	ExpressionPtr initializer;
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
struct FieldAttributes {
	// [[position]]: the field is the stage's own position. On a vertex output
	// that is the BuiltIn Position; on a fragment input it is the FragCoord.
	bool position = false;

	// [[attribute(n)]]: the field is a vertex buffer input or a user output at
	// location n.
	std::optional<uint32_t> attributeIndex;
};

struct StructField {
	Type type;
	std::string name;
	FieldAttributes attributes;
};

struct StructDecl {
	std::string name;
	std::vector<StructField> fields;
};

struct FunctionDecl {
	Stage stage = Stage::None;
	Type returnType;
	std::string name;
	std::vector<Parameter> parameters;
	StatementPtr body;
	size_t line = 0;
};

struct TranslationUnit {
	std::vector<StructDecl> structs;
	std::vector<FunctionDecl> functions;

	// MSL entry points, keyed by function name.
	const FunctionDecl* findFunction(const std::string& name) const;
	const StructDecl* findStruct(const std::string& name) const;
};

}
