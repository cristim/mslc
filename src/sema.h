#pragma once

#include "ast.h"
#include "spirv.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
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

public:
	struct MatrixInfo {
		ScalarKind scalar;
		uint32_t columns;
		uint32_t rows;
		// The column vector type, which is what indexing a matrix yields.
		spirv::Id column;
	};

private:
	std::map<std::tuple<uint32_t, uint32_t, uint32_t>, spirv::Id> _matrices;
	std::map<spirv::Id, MatrixInfo> _matrixInfo;
	std::map<std::pair<spirv::StorageClassValue, spirv::Id>, spirv::Id> _pointers;
	std::map<std::string, spirv::Id> _structs;
	std::map<std::string, spirv::Id> _valueStructs;
	std::map<std::string, spirv::Id> _elementStructs;
	// The size in bytes of each array-element struct, which is the stride an
	// array of that struct steps by.
	std::map<spirv::Id, uint32_t> _structSizes;
	std::map<std::pair<spirv::Id, bool>, spirv::Id> _blockStructs;
	std::map<spirv::Id, spirv::Id> _bufferPointers;

	std::map<std::pair<uint32_t, uint32_t>, spirv::Id> _packedStorage;
	bool _storage8Declared = false;

	spirv::Id _voidType = spirv::InvalidId;
	std::map<ResourceKind, spirv::Id> _images;
	std::map<ResourceKind, spirv::Id> _sampledImages;
	spirv::Id _samplerType = spirv::InvalidId;
	spirv::Id _uintWriteImage = spirv::InvalidId;

	// A matrix member of a laid-out struct needs ColMajor and MatrixStride, which
	// SPIR-V only allows on a struct member, so every struct with an Offset on a
	// matrix member goes through this. Does nothing for any other member type.
	void decorateMatrixMember(spirv::Id structure, uint32_t member, spirv::Id type);

	std::vector<spirv::Id> storageTypesFor(const std::string& name,
		const std::vector<spirv::Id>& valueTypes);

public:
	TypeTable(spirv::Builder& builder, const TranslationUnit& unit):
		_builder(builder), _unit(unit) {}

	spirv::Id voidType();
	spirv::Id scalar(ScalarKind kind);
	std::optional<ScalarKind> scalarKindOf(spirv::Id type) const;
	spirv::Id vector(ScalarKind kind, uint32_t width);
	spirv::Id pointer(spirv::StorageClassValue storageClass, spirv::Id pointee);
	spirv::Id matrix(ScalarKind kind, uint32_t columns, uint32_t rows);

	// The image a texture2d, texturecube or texture3d of float or half is declared
	// as: a float sampled type, sampled by a sampler (Sampled 1), format Unknown and
	// depth unspecified, in Dim 2D, Cube or 3D, which is what Iridium declares (indium
	// src/iridium/air.cpp:771-785). The component type is not in the image type;
	// a half texture's samples are narrowed after the lookup.
	spirv::Id image(ResourceKind kind);
	spirv::Id uintWriteImage();
	spirv::Id samplerType();
	spirv::Id sampledImage(ResourceKind kind);

	// How a packed vector of the given type is stored in a laid-out struct or
	// buffer: an array of its components, strided by the component's size.
	spirv::Id packedStorage(ScalarKind kind, uint32_t width);

	// How a bool or a bool vector is held in a buffer: a byte, or an array of
	// bytes, one per lane. Declares the 8-bit storage capability the first time.
	spirv::Id boolStorage(spirv::Id boolType);

	// The shape of a matrix, or null when the id is not a matrix this table
	// created.
	const MatrixInfo* matrixInfo(spirv::Id type) const;

	// What kind of type an id is. The table knows these because it created
	// them, which is why they are asked here rather than inferred at each use
	// site from an AST that does not carry resolved types.
	// The type a pointer type points at, or InvalidId when the id is not a
	// pointer this table created. An assignment needs this to know what type a
	// store's value must have.
	spirv::Id pointeeOf(spirv::Id pointerType) const;

	// The storage class of a pointer type, or InvalidId when the id is not a
	// pointer this table created. Asking is not the same as building a pointer
	// type to compare against, which would declare one as a side effect.
	std::optional<spirv::StorageClassValue> storageClassOf(spirv::Id pointerType) const;

	bool isFloat(spirv::Id type) const;
	bool isBool(spirv::Id type) const;
	bool isSignedInt(spirv::Id type) const;
	uint32_t vectorWidth(spirv::Id type) const;

	// Bit width of a scalar or of a vector's component. 0 for a type that is
	// neither, so a caller comparing two widths can tell them apart.
	uint32_t bitWidth(spirv::Id type) const;

	// The component of a vector, or InvalidId when the id is not a vector this
	// table created. A broadcast needs it: the value has to be converted to the
	// component's own type before it goes in every component, or a float would be
	// reinterpreted as an int.
	spirv::Id componentOf(spirv::Id vectorType);

	// The vector of the given width with the same component, or that component
	// for a width of 1. InvalidId when the id is not a vector this table created.
	spirv::Id withWidth(spirv::Id vectorType, uint32_t width);

	// What a struct type id is: which declared struct it is a form of, the member
	// types that form has, and the member types of the plain value form. They
	// differ for a packed vector, which a laid-out form holds as an array of its
	// components. False when the id is not a struct this table created.
	struct StructForm {
		std::string name;
		std::vector<spirv::Id> declared;
		std::vector<spirv::Id> value;
	};
	bool structForm(spirv::Id type, StructForm& out);
	// The declared struct's name a type id is a form of, or null; laidOut says
	// whether it is a buffer's form.
	const std::string* structNameOf(spirv::Id type, bool* laidOut) const;

	// True when the type is a vector or a struct, which a single scalar value
	// cannot stand in for.
	bool isAggregate(spirv::Id type) const;

	// The zero value of a type, for a local declared without an initialiser. Each
	// scalar kind needs its own form: a bool has separate opcodes, and
	// OpConstant's literal count follows the width, so a 64-bit value needs two
	// words. A vector, a matrix or a struct is OpConstantNull.
	spirv::Id zero(spirv::Id type);

	// One in a numeric scalar or vector type, in every component. A bool
	// converts to this where it is true.
	spirv::Id one(spirv::Id type);

	// A struct by name. Returns InvalidId when the name is not declared in the
	// unit.
	spirv::Id namedStruct(const std::string& name);

	// The same struct as a value rather than as a buffer's block: the same
	// members, with no Block decoration and no member offsets, since a struct
	// used as a constant or an interface member is laid out by whatever holds
	// it. InvalidId when the name is not declared.
	spirv::Id valueStruct(const std::string& name);

	// The member types of a struct, where each one starts in a buffer, and the
	// struct's own size, laid out the way Metal lays it out. False when the name
	// is not declared.
	bool structMembersFor(const std::string& name, std::vector<spirv::Id>& outTypes,
		std::vector<uint32_t>& outOffsets, uint32_t& outSize);

	// The same struct as the element of an array of it: the members with their
	// offsets, and no Block decoration, since Vulkan requires a struct nested in
	// a Block to be laid out and rejects a Block-decorated one inside an array.
	spirv::Id arrayElementStruct(const std::string& name);

	// The Block-decorated struct that wraps a buffer's elements, as
	// { T runtime_array[] }. It has to be a struct rather than a bare T because
	// OpAccessChain rejects a non-composite base, and a PhysicalStorageBuffer
	// pointer therefore cannot point straight at an element. Idempotent per
	// element type and layout. A packed vector element steps by its components'
	// total size, and an unpacked one by Metal's rule, which rounds a 3-vector up.
	spirv::Id blockStructFor(spirv::Id elementType, bool packed);

	// A pointer to a pointee in a buffer's own address space, which is what the
	// binding-0 address block hands the shader. Idempotent per pointee.
	spirv::Id bufferPointer(spirv::Id pointee);

	// The binding-0 block: a Block-decorated struct whose members are the buffer
	// pointers, one per buffer parameter in declaration order, each at offset
	// 8 * k, and the Uniform variable that holds it. The shader loads a member to
	// get a buffer's address, which is what indium fills in.
	struct AddressBlock {
		spirv::Id blockType = spirv::InvalidId;
		spirv::Id variable = spirv::InvalidId;
		spirv::Id memberPointer = spirv::InvalidId;
		// What its members point at, in order.
		std::vector<spirv::Id> pointeeTypes;
	};
	// Builds the binding-0 address block for one descriptor set and member list
	// on the first call for that pair, and returns it unchanged afterwards. Keyed
	// by set because indium splits the sets by stage, and by member list because
	// two entry points of one stage with different buffers cannot share one typed
	// block; each gets its own variable at the same set and binding, and only the
	// entry point a pipeline is created from uses it.
	AddressBlock addressBlock(const std::vector<spirv::Id>& pointeeTypes,
		uint32_t descriptorSet);

	std::map<std::pair<uint32_t, std::vector<spirv::Id>>, AddressBlock> _addressBlocks;

	// The address a buffer's own storage class guarantees for one access, which
	// is what the Aligned memory operand has to say. A buffer element sits at a
	// multiple of its own size, so that is the whole guarantee, and claiming more
	// is what lets a compiler read past a buffer. A packed vector is only as
	// aligned as its component.
	uint32_t alignmentOf(spirv::Id type, bool packed) const;

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

// The entry points to compile, in declaration order. A requested stage selects
// every function of that stage, and an unspecified stage selects every entry
// point in the unit. Throws CompileError when the source declares none of the
// requested stage, or none at all, rather than emitting a module with no entry
// point, which no pipeline can be created from.
std::vector<const FunctionDecl*> selectEntryPoints(const TranslationUnit& unit, Stage requested);

// Options that affect code generation rather than parsing.
struct ModuleOptions {
	uint32_t localSizeX = 1;
	uint32_t localSizeY = 1;
	uint32_t localSizeZ = 1;
	bool separateImageSet = false;
};

// The reflection's embedded_samplers entries carry the whole of indium's
// EmbeddedSampler. The options that set compare_function, anisotropy,
// border_color and the lod clamp are refused, so those fields hold Apple's
// defaults for a sampler that does not set them: compare_function Never (8 in
// Apple's sampler state word), anisotropy 1, border_color TransparentBlack,
// lod_min 0 and lod_max 65504 (the largest half). A consumer must not zero-fill
// them: lod_max 0 would restrict the sampler to mip level 0.
//
// A vertex function's [[stage_in]] struct adds one binding per field, in
// declaration order: { "kind": "VertexInput", "metal_index": n, "location": n,
// "name": field }. n is the field's [[attribute(n)]], the key of indium's vertex
// descriptor attribute, and is also the Location of the Input variable. On this
// entry metal_index is an attribute index, never a [[buffer(n)]] index.
// reflection_version stays 2 because bindings are additive: a consumer switches
// on "kind" and ignores kinds it does not know.
//
// Emits the module for every given entry point: one OpEntryPoint and one
// function each, with the descriptor set, the execution modes and the interface
// chosen per entry point's stage. Returns the reflection document, one entry per
// entry point, in the same order. Throws CompileError for anything the subset
// cannot represent, naming the construct.
std::string emitModule(spirv::Builder& builder, const TranslationUnit& unit,
	const std::vector<const FunctionDecl*>& entryPoints, const ModuleOptions& options);

}
