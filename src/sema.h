#pragma once

#include "ast.h"
#include "spirv.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mslc {

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
	std::map<std::pair<spirv::StorageClassValue, spirv::Id>, spirv::Id> _pointers;
	std::map<std::string, spirv::Id> _structs;
	std::map<std::string, spirv::Id> _valueStructs;
	std::map<std::pair<uint32_t, uint32_t>, spirv::Id> _images;
	std::map<std::vector<spirv::Id>, spirv::Id> _functionTypes;
	std::map<spirv::Id, spirv::Id> _blockStructs;

	spirv::Id _voidType = spirv::InvalidId;
	spirv::Id _samplerType = spirv::InvalidId;

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

	// What kind of type an id is. The table knows these because it created
	// them, which is why they are asked here rather than inferred at each use
	// site from an AST that does not carry resolved types.
	// The type a pointer type points at, or InvalidId when the id is not a
	// pointer this table created. An assignment needs this to know what type a
	// store's value must have.
	spirv::Id pointeeOf(spirv::Id pointerType) const;

	bool isFloat(spirv::Id type) const;
	bool isSignedInt(spirv::Id type) const;
	uint32_t vectorWidth(spirv::Id type) const;

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

	// The same struct as a descriptor payload: Block-decorated with a per-member
	// Offset, which is what Vulkan requires of a struct behind a buffer binding
	// and what Metal's own layout has to be expressed as. A distinct type from
	// namedStruct, since the decorations cannot be undone.
	spirv::Id blockStruct(const std::string& name);

	// A struct's member types and their offsets. Throws for a field whose type
	// mslc cannot represent.
	bool structMembers(const StructDecl& decl, std::vector<spirv::Id>& outTypes,
		std::vector<uint32_t>& outOffsets);

	// How many bytes one scalar of a kind occupies.
	uint32_t fieldTypeSize(ScalarKind kind);

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
