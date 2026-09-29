#pragma once

#include "ast.h"
#include "spirv.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace mslc {

// How many bytes a vector occupies in a buffer and what it aligns to, which is
// Metal's layout and not its component count. Recorded where the type is
// declared so that a buffer's element stride is read off the type rather than
// re-derived from the scalar it is built from.
struct VectorLayout {
	uint32_t size;
	uint32_t alignment;
};

// What a struct occupies in a buffer, which is what an array of it steps by and
// what decides where its next member starts. Metal's own numbers, not the
// packed widths the components have on their own: a float3 member is 16 bytes
// of which 12 are the three floats, so a struct of one is 16 and not 12.
struct StructLayout {
	uint32_t size;
	uint32_t alignment;
};

// A SPIR-V type the emitter has already declared, tracked so that a repeated
// type is declared once. Metal and SPIR-V spell types differently often enough
// that the emitter needs this cache rather than re-deriving per use.
class TypeTable {
	spirv::Builder& _builder;
	const TranslationUnit& _unit;

	std::map<uint32_t, spirv::Id> _scalars;
	std::map<spirv::Id, uint32_t> _widthOfScalar;
	std::map<std::pair<uint32_t, uint32_t>, spirv::Id> _vectors;
	std::map<spirv::Id, uint32_t> _widthOfVector;
	// How many bytes each vector occupies in a buffer and what it aligns to.
	std::map<spirv::Id, VectorLayout> _layoutOfVector;
	// Keyed by the component, the column width and the column count, in that
	// order, so a float4x4 and a float4x3 do not collide.
	std::map<std::tuple<uint32_t, uint32_t, uint32_t>, spirv::Id> _matrices;
	// The column type of each matrix, which is also the type its rows are a
	// vector of: "float4x4" is four columns of float4.
	std::map<spirv::Id, spirv::Id> _columnTypeOfMatrix;
	std::map<std::pair<spirv::StorageClassValue, spirv::Id>, spirv::Id> _pointers;
	std::map<std::string, spirv::Id> _structs;
	std::map<std::string, spirv::Id> _valueStructs;
	std::map<std::string, spirv::Id> _elementStructs;
	// What each struct occupies in a buffer, which is the stride an array of
	// that struct has.
	std::map<spirv::Id, StructLayout> _structLayouts;
	std::map<std::pair<uint32_t, uint32_t>, spirv::Id> _images;
	std::map<spirv::Id, spirv::Id> _sampledImages;
	std::map<std::vector<spirv::Id>, spirv::Id> _functionTypes;
	std::map<spirv::Id, spirv::Id> _blockStructs;

	spirv::Id _voidType = spirv::InvalidId;
	spirv::Id _samplerType = spirv::InvalidId;

	// A struct's member types and where each one starts in a buffer. outLayout,
	// when given, receives what the struct itself occupies, which is how far an
	// array of it steps. outStride, when given, receives for each member the
	// stride between its own columns, which SPIR-V requires as a MatrixStride
	// for a member that is or holds a matrix. False when the name is not a
	// struct this source declares.
	bool structMembersFor(const std::string& name, std::vector<spirv::Id>& outTypes,
		std::vector<uint32_t>& outOffsets, std::vector<uint32_t>* outStride,
		StructLayout* outLayout);

	// How many bytes one scalar of a kind occupies.
	uint32_t fieldTypeSize(ScalarKind kind);

	// The decorations a laid-out struct's matrix member needs, or nothing when
	// the member is not a matrix. SPIR-V rejects a laid-out struct with a matrix
	// member that has neither: the stride between its columns is what tells the
	// runtime how to step through it, and the row-major or column-major is what
	// says which way it steps.
	void decorateMatrixStride(spirv::Id structure, size_t member, uint32_t stride);

public:
	TypeTable(spirv::Builder& builder, const TranslationUnit& unit):
		_builder(builder), _unit(unit) {}

	spirv::Id voidType();

	// SPIR-V's sampler type. Declared once however many samplers the source
	// names, since a module may only declare a non-aggregate type once.
	spirv::Id sampler();
	spirv::Id scalar(ScalarKind kind);
	spirv::Id vector(ScalarKind kind, uint32_t width);
	spirv::Id pointer(spirv::StorageClassValue storageClass, spirv::Id pointee);

	// A matrix, as SPIR-V spells one: a vector-typed column and a count of them.
	// Metal and SPIR-V are both column-major, so a column is the unit a matrix
	// is built and read in, and the mapping is the identity on the order the
	// columns are given in. Emits the Matrix capability, which a MatrixStride
	// decoration needs and OpTypeMatrix does not.
	spirv::Id matrix(ScalarKind kind, uint32_t columns, uint32_t rows);

	// The column type a matrix type is made of, which is also what a product of
	// that matrix with a vector comes out as. InvalidId when the id is not a
	// matrix this table created, which is how a matrix is told apart from a
	// vector or a struct.
	spirv::Id matrixColumnType(spirv::Id matrixType) const;

	// How many columns a matrix type has, or 0 when the id is not a matrix. A
	// matrix is declared as a column and a count of them, so this is what
	// distinguishes a float4x4 from a float3x4.
	uint32_t matrixColumnCount(spirv::Id matrixType) const;

	// What kind of type an id is. The table knows these because it created
	// them, which is why they are asked here rather than inferred at each use
	// site from an AST that does not carry resolved types.
	// The type a pointer type points at, or InvalidId when the id is not a
	// pointer this table created. An assignment needs this to know what type a
	// store's value must have.
	spirv::Id pointeeOf(spirv::Id pointerType) const;

	// The scalar a vector type is built from, or ScalarKind::Void when the id is
	// not a vector this table created. A swizzle's result type follows from it,
	// since a component of a uint vector is a uint.
	ScalarKind componentKind(spirv::Id vectorType) const;

	bool isFloat(spirv::Id type) const;
	bool isSignedInt(spirv::Id type) const;
	uint32_t vectorWidth(spirv::Id type) const;

	// The component type an image yields, or ScalarKind::Void when the id is not
	// an image this table created. A read of a sampled image is a whole texel,
	// so a float texture's is four components whatever the shader keeps of it.
	ScalarKind imageComponent(spirv::Id imageType) const;

	// The type combining an image with a sampler, which is what a sample reads
	// through. Declared once per image type, since a module may declare a
	// non-aggregate type only once.
	spirv::Id sampledImageOf(spirv::Id imageType);

	// The sampled image a Metal texture type maps onto: the component type is
	// the scalar, and the Sampled operand is 1 because a Metal texture is read
	// through a sampler rather than stored to directly. Idempotent per
	// component type and shape.
	spirv::Id sampledImage(ScalarKind component, TextureDim dim);

	// The type of a function, declared once per distinct signature: a module may
	// declare a non-aggregate type only once, and every entry point with the
	// same signature shares one.
	spirv::Id functionType(spirv::Id returnType, const std::vector<spirv::Id>& parameterTypes);

	// A struct by name, as a value: a local or the contents of another struct.
	// It carries no buffer layout, since a value is not laid out in a buffer.
	// Returns InvalidId when the name is not declared in the unit.
	spirv::Id namedStruct(const std::string& name);

	// The struct an array of this struct holds. A third form, laid out but not
	// Block-decorated: Vulkan requires a struct nested inside a Block to be
	// explicitly laid out, and rejects one that is Block-decorated inside an
	// array, so neither of the other two forms can be the element. Returns
	// InvalidId when the name is not declared in the unit.
	spirv::Id arrayElementStruct(const std::string& name);

	// The same struct as a descriptor payload: Block-decorated with a per-member
	// Offset, which is what Vulkan requires of a struct behind a buffer binding
	// and what Metal's own layout has to be expressed as. A distinct type from
	// namedStruct, since the decorations cannot be undone.
	spirv::Id blockStruct(const std::string& name);

	// The Block-decorated struct that wraps a buffer's elements, as
	// { T runtime_array[] }. Vulkan only accepts a struct for a StorageBuffer or
	// Uniform descriptor, so a Metal "device T* parameter" becomes a descriptor
	// over this rather than a pointer to T directly. Idempotent per element
	// type.
	spirv::Id blockStructFor(spirv::Id elementType);

	// True when the type needs a capability beyond Shader, and emits it.
	void requireCapabilitiesFor(const Type& type);
};

// Which SPIR-V storage class a parameter's Metal address space maps onto.
std::optional<spirv::StorageClassValue> storageClassForAddressSpace(AddressSpace space);

// What the emitter needs to know about one resolved entry-point parameter.
struct ResolvedParameter {
	const Parameter* parameter = nullptr;
	spirv::Id valueId = spirv::InvalidId;   // the id standing for the parameter
	spirv::Id valueType = spirv::InvalidId;
	spirv::Id pointeeType = spirv::InvalidId; // element type, for pointer parameters
	uint32_t binding = UINT32_MAX;
	spirv::StorageClassValue storageClass = spirv::StorageClass::StorageBuffer;
	bool isBuiltin = false;
	bool isImage = false;
	bool isSampler = false;
};

// The emitter's view of one entry point, produced by analysing the AST.
struct ResolvedEntryPoint {
	const FunctionDecl* function = nullptr;
	Stage stage = Stage::None;
	std::vector<ResolvedParameter> parameters;
	spirv::Id returnType = spirv::InvalidId;
};

// Finds the entry points to compile: the one matching the requested stage, or
// every entry point in the unit when the stage is unspecified. A Metal source
// is a library rather than a function, so a shader that declares a vertex and a
// fragment function produces one module with both, which is what compiling
// through newLibraryWithSource: yields. Throws CompileError when a requested
// stage is absent or the source declares no entry point at all.
std::vector<const FunctionDecl*> selectEntryPoints(const TranslationUnit& unit, Stage requested);

// Options that affect code generation rather than parsing.
struct ModuleOptions {
	uint32_t localSizeX = 1;
	uint32_t localSizeY = 1;
	uint32_t localSizeZ = 1;
	bool separateImageSet = false;
};

// One entry point as mslc reports it: its stage, its name, and the descriptor
// bindings its own resources were given. A module has as many of these as its
// source declared entry points.
struct EmittedEntryPoint {
	Stage stage = Stage::None;
	std::string name;
	std::string bindings;
};

// What to report about a module: the bindings the module itself owns, and its
// entry points in order.
struct EmittedModule {
	std::string moduleBindings;
	std::vector<EmittedEntryPoint> entries;
};

// Emits a module holding every given entry point, and returns what to report
// about it. Throws CompileError for anything the subset cannot represent,
// naming the construct.
EmittedModule emitModule(spirv::Builder& builder, const TranslationUnit& unit,
	const std::vector<const FunctionDecl*>& entryPoints, const ModuleOptions& options);

}
