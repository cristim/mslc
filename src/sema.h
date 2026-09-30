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
	std::map<spirv::Id, spirv::Id> _blockStructs;
	std::map<spirv::Id, spirv::Id> _bufferPointers;

	spirv::Id _voidType = spirv::InvalidId;

public:
	TypeTable(spirv::Builder& builder, const TranslationUnit& unit):
		_builder(builder), _unit(unit) {}

	spirv::Id voidType();
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

	// The storage class of a pointer type, or InvalidId when the id is not a
	// pointer this table created. Asking is not the same as building a pointer
	// type to compare against, which would declare one as a side effect.
	std::optional<spirv::StorageClassValue> storageClassOf(spirv::Id pointerType) const;

	bool isFloat(spirv::Id type) const;
	bool isSignedInt(spirv::Id type) const;
	uint32_t vectorWidth(spirv::Id type) const;

	// Bit width of a scalar or of a vector's component. 0 for a type that is
	// neither, so a caller comparing two widths can tell them apart.
	uint32_t bitWidth(spirv::Id type) const;

	// True when the type is a vector or a struct, which a single scalar value
	// cannot stand in for.
	bool isAggregate(spirv::Id type) const;

	// The zero value of a scalar type, for a local declared without an
	// initialiser. Each scalar kind needs its own form: a bool has separate
	// opcodes, and OpConstant's literal count follows the width, so a 64-bit
	// value needs two words.
	spirv::Id zero(spirv::Id type);

	// A struct by name. Returns InvalidId when the name is not declared in the
	// unit.
	spirv::Id namedStruct(const std::string& name);

	// The same struct as a value rather than as a buffer's block: the same
	// members, with no Block decoration and no member offsets, since a struct
	// used as a constant or an interface member is laid out by whatever holds
	// it. InvalidId when the name is not declared.
	spirv::Id valueStruct(const std::string& name);

	// The member types of a struct and where each one starts in a buffer, laid
	// out the way Metal lays it out. False when the name is not declared.
	bool structMembersFor(const std::string& name, std::vector<spirv::Id>& outTypes,
		std::vector<uint32_t>& outOffsets);

	// The Block-decorated struct that wraps a buffer's elements, as
	// { T runtime_array[] }. It has to be a struct rather than a bare T because
	// OpAccessChain rejects a non-composite base, and a PhysicalStorageBuffer
	// pointer therefore cannot point straight at an element. Idempotent per
	// element type.
	spirv::Id blockStructFor(spirv::Id elementType);

	// A pointer to a pointee in a buffer's own address space, which is what the
	// binding-0 address block hands the shader. Idempotent per pointee.
	spirv::Id bufferPointer(spirv::Id pointee);

	// The binding-0 block: a Block-decorated struct whose members are the buffer
	// pointers, one per buffer parameter in declaration order, each at offset
	// 8 * k, and the Uniform variable that holds it. The shader loads a member to
	// get a buffer's address, which is what indium fills in.
	// The binding-0 block and the variable that holds it. Named here because the
	// emitter walks the block to reach a buffer's address.
	struct AddressBlock {
		spirv::Id blockType = spirv::InvalidId;
		spirv::Id variable = spirv::InvalidId;
		spirv::Id memberPointer = spirv::InvalidId;
	};
	// Builds the module's one binding-0 address block from the given pointee
	// types on first call and returns it unchanged afterwards, decorating its
	// variable with the given descriptor set.
	AddressBlock addressBlock(const std::vector<spirv::Id>& pointeeTypes, uint32_t descriptorSet);

	AddressBlock _addressBlock;

	// The address a buffer's own storage class guarantees for one access, which
	// is what the Aligned memory operand has to say. A buffer element sits at a
	// multiple of its own size, so that is the whole guarantee, and claiming more
	// is what lets a compiler read past a buffer.
	uint32_t alignmentOf(spirv::Id type) const;

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
