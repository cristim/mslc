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
	std::map<std::pair<uint32_t, uint32_t>, spirv::Id> _images;
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

	// A struct by name. Returns InvalidId when the name is not declared in the
	// unit.
	spirv::Id namedStruct(const std::string& name);

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

// Finds the entry point to compile: the one matching the requested stage, or the
// single entry point in the unit when the stage is unspecified. Returns nullptr
// when the choice is ambiguous, which is an error the caller must report rather
// than guess at.
const FunctionDecl* selectEntryPoint(const TranslationUnit& unit, Stage requested);

// Options that affect code generation rather than parsing.
struct ModuleOptions {
	uint32_t localSizeX = 1;
	uint32_t localSizeY = 1;
	uint32_t localSizeZ = 1;
	bool separateImageSet = false;
};

// Emits the module for one entry point. Throws CompileError for anything the
// subset cannot represent, naming the construct.
std::string emitModule(spirv::Builder& builder, const TranslationUnit& unit,
	const FunctionDecl& entryPoint, const ModuleOptions& options);

}
