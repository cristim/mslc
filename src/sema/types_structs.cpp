#include "sema/emitter.h"

namespace mslc {

uint32_t TypeTable::alignmentOf(Id type, bool packed) const {
	const uint32_t bits = bitWidth(type);
	if (bits == 0 && storageClassOf(type)) {
		// A pointer held in a buffer, as a struct's pointer member is: 8 bytes.
		return 8;
	}
	// An array is as aligned as its element: an array of structs holding a long is 8-aligned, not 4.
	if (const Id element = bits == 0 ? arrayElement(type) : InvalidId; element != InvalidId) {
		return alignmentOf(element, packed);
	}
	if (bits == 0) {
		// A struct is as aligned as its most aligned member, which spirv-val needs
		// to be at least its largest scalar: 8 for a long.
		const std::string* name = structNameOf(type, nullptr);
		const StructDecl* decl = name ? _unit.findStruct(*name) : nullptr;
		if (!decl) {
			return 4;
		}

		uint32_t alignment = 1;
		for (const StructField& field: decl->fields) {
			alignment = std::max(alignment, memberLayout(field.type, &_unit).alignment);
		}

		return alignment;
	}

	// bitWidth is the component's width, so the element's own size comes from the
	// component's size and the component count. Rounded up, so a 24-bit
	// component claims 4, and taken from Metal's own rule rather than from
	// (bits * count) / 8, because a float3 is 16 bytes and 16-aligned where its
	// three floats are 12. Aligned has to name a power of two, which the Metal
	// rule gives and a size does not.
	const auto it = _widthOfVector.find(type);
	const uint32_t count = it != _widthOfVector.end() ? it->second : 1;
	const uint32_t scalarBytes = (bits + 7) / 8;
	if (packed) {
		return std::max<uint32_t>(1, packedVectorLayoutFor(scalarBytes, count).alignment);
	}

	return std::max<uint32_t>(1, vectorLayoutFor(scalarBytes, count).alignment);
}

Id TypeTable::zero(Id type) {
	// OpConstant is a scalar form; OpConstantNull zeroes every component and
	// member of a vector, matrix or struct.
	if (isAggregate(type)) {
		return _builder.emitDeclTyped(spirv::OpConstantNull, type, { });
	}

	// A bool has no literal form, so it needs its own opcode rather than a zero
	// word, which the grammar does not accept for it.
	if (type == scalar(ScalarKind::Bool)) {
		return _builder.emitDeclTyped(spirv::OpConstantFalse, type, { });
	}

	// OpConstant's literal count is context dependent, so it follows the width:
	// one word up to 32 bits, two for 64. Writing one word for a 64-bit value is
	// a short instruction, which is what this used to do.
	return _builder.emitDeclTyped(spirv::OpConstant, type,
		std::vector<uint32_t>((bitWidth(type) + 31) / 32, 0u));
}

Id TypeTable::one(Id type) {
	const Id vectorComponent = componentOf(type);
	const Id component = vectorComponent == InvalidId ? type : vectorComponent;
	const uint32_t bits = bitWidth(component);

	std::vector<uint32_t> words;
	if (isFloat(component)) {
		if (bits == 64) {
			throw CompileError("double is not supported");
		}

		words = { bits == 16 ? 0x3C00u : 0x3F800000u };
	} else {
		words = std::vector<uint32_t>((bits + 31) / 32, 0u);
		words.front() = 1u;
	}

	const Id scalarOne = _builder.emitDeclTyped(spirv::OpConstant, component, words);
	if (component == type) {
		return scalarOne;
	}

	return _builder.emitDeclTyped(spirv::OpConstantComposite, type,
		std::vector<uint32_t>(vectorWidth(type), scalarOne));
}

void TypeTable::requireCapability(spirv::CapabilityValue capability) {
	if (_capabilities.insert(static_cast<uint32_t>(capability)).second) {
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(capability) });
	}
}

// The Dim, Arrayed flag and capability a texture kind's image type takes; the
// sampled and the storage image of a kind differ only in the Sampled operand.
Id TypeTable::imageOf(ResourceKind kind, ScalarKind sampledType, uint32_t sampled, bool depth, bool multisampled) {
	const bool cube = kind == ResourceKind::TextureCube || kind == ResourceKind::TextureCubeArray;
	const bool oneD = kind == ResourceKind::Texture1D || kind == ResourceKind::Texture1DArray;
	const bool arrayed = kind == ResourceKind::Texture2DArray || kind == ResourceKind::Texture1DArray
		|| kind == ResourceKind::TextureCubeArray;
	const bool buffer = kind == ResourceKind::TextureBuffer;
	if (oneD) {
		requireCapability(sampled == 2 ? spirv::Capability::Image1D : spirv::Capability::Sampled1D);
	}
	if (buffer) {
		requireCapability(sampled == 2 ? spirv::Capability::ImageBuffer : spirv::Capability::SampledBuffer);
	}
	if (kind == ResourceKind::TextureCubeArray) {
		requireCapability(spirv::Capability::ImageCubeArray);
	}
	if (multisampled && arrayed && sampled == 2) {
		requireCapability(spirv::Capability::ImageMSArray);
	}
	return _builder.emitDecl(spirv::OpTypeImage, { scalar(sampledType),
		cube ? spirv::Dim::Cube : kind == ResourceKind::Texture3D ? spirv::Dim::Dim3D
			: buffer ? spirv::Dim::Buffer : oneD ? spirv::Dim::Dim1D : spirv::Dim::Dim2D,
		depth ? 1u : sampled == 2 ? 0u : 2u, arrayed ? 1u : 0u, multisampled ? 1u : 0u, sampled,
		static_cast<uint32_t>(spirv::ImageFormat::Unknown) });
}

Id TypeTable::image(ResourceKind kind, ScalarKind sampledType, bool depth, bool multisampled) {
	Id& cached = _images[{ kind, sampledType, depth, multisampled }];
	if (cached == InvalidId) {
		cached = imageOf(kind, sampledType, 1u, depth, multisampled);
	}

	return cached;
}

Id TypeTable::storageImage(ResourceKind kind, TextureAccess access, ScalarKind element) {
	if (access != TextureAccess::Write && !_storageReadDeclared) {
		_storageReadDeclared = true;
		_builder.emit(spirv::OpCapability,
			{ static_cast<uint32_t>(spirv::Capability::StorageImageReadWithoutFormat) });
	}
	if (access != TextureAccess::Read && !_storageWriteDeclared) {
		_storageWriteDeclared = true;
		_builder.emit(spirv::OpCapability,
			{ static_cast<uint32_t>(spirv::Capability::StorageImageWriteWithoutFormat) });
	}
	Id& cached = _storageImages[{ kind, element }];
	if (cached == InvalidId) {
		cached = imageOf(kind, element, 2u, false, false);
	}
	return cached;
}

Id TypeTable::subpassImage(ScalarKind element) {
	Id& cached = _subpassImages[element];
	if (cached == InvalidId) {
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::InputAttachment) });
		cached = _builder.emitDecl(spirv::OpTypeImage, { scalar(element),
			spirv::Dim::SubpassData, 0u, 0u, 0u, 2u,
			static_cast<uint32_t>(spirv::ImageFormat::Unknown) });
	}
	return cached;
}

Id TypeTable::samplerType() {
	if (_samplerType == InvalidId) {
		_samplerType = _builder.emitDecl(spirv::OpTypeSampler);
	}

	return _samplerType;
}

Id TypeTable::sampledImage(ResourceKind kind, ScalarKind sampledType, bool depth, bool multisampled) {
	const Id imageType = image(kind, sampledType, depth, multisampled);
	Id& cached = _sampledImages[{ kind, sampledType, depth, multisampled }];
	if (cached == InvalidId) {
		cached = _builder.emitDecl(spirv::OpTypeSampledImage, { imageType });
	}

	return cached;
}

Id TypeTable::pointer(spirv::StorageClassValue storageClass, Id pointee) {
	const auto key = std::make_pair(storageClass, pointee);
	const auto cached = _pointers.find(key);
	if (cached != _pointers.end()) {
		return cached->second;
	}

	const Id id = _builder.emitDecl(spirv::OpTypePointer,
		{ static_cast<uint32_t>(storageClass), pointee });

	_pointers.emplace(key, id);
	return id;
}

// The member types of a struct and, where the caller wants them, where each one
// starts in a buffer. Metal lays a struct out with every member at the next
// multiple of its own alignment, so a float3 member both starts and steps 16
// bytes even though it holds 12, which is what a float member after it has to
// account for.
//
// False when no struct of that name is declared, so a caller can say so rather
// than guess.
bool TypeTable::structMembersFor(const std::string& name, std::vector<Id>& outTypes,
	std::vector<uint32_t>& outOffsets, uint32_t& outSize) {

	const StructDecl* decl = _unit.findStruct(name);
	if (!decl) {
		return false;
	}

	uint64_t offset = 0;
	uint32_t alignment = 1;

	for (const StructField& field: decl->fields) {
		const bool nested = !field.type.isPointer && !field.type.namedType.empty();
		if (nested && _unit.findStruct(field.type.namedType) == decl) {
			throw CompileError("struct \"" + decl->name + "\" contains itself (\"" + field.name + "\")");
		}

		if (field.type.arrayLength && (field.type.isPacked || field.type.scalar == ScalarKind::Bool)) {
			throw CompileError("the array member \"" + field.name + "\" of \"" + decl->name + "\" is an array of "
				+ (field.type.isPacked ? "a packed vector" : "bool") + ", which mslc does not lay out yet");
		}
		if (field.type.arrayLength && field.type.isPointer) {
			throw CompileError("the array member \"" + field.name + "\" of \"" + decl->name
				+ "\" is an array of pointers, which mslc does not lower");
		}
		if (field.type.isPointer && field.type.addressSpace != AddressSpace::Device
			&& field.type.addressSpace != AddressSpace::Constant) {
			throw CompileError("a pointer member (\"" + field.name + "\" of \"" + decl->name
				+ "\") has to point at device or constant memory to be lowered");
		}
		if (field.type.isPointer && (!field.type.namedType.empty() || field.type.isAtomic)) {
			throw CompileError("a pointer member (\"" + field.name + "\" of \"" + decl->name
				+ "\") to a struct or to atomics is not lowered yet");
		}

		const uint32_t components = field.type.vectorWidth > 1 ? field.type.vectorWidth : 1;
		const VectorLayout layout = memberLayout(field.type, &_unit);
		// A struct member is the struct's value form here; storageTypesFor gives its laid-out form.
		Id valueType = nested ? valueStruct(field.type.namedType)
			: field.type.isMatrix()
			? matrix(field.type.scalar, field.type.matrixColumns, field.type.vectorWidth)
			: components > 1
			? vector(field.type.scalar, field.type.vectorWidth)
			: scalar(field.type.scalar);
		// A device or constant pointer member holds a buffer's GPU address, so it is
		// the pointer a pointer parameter is: to the { T runtime_array[] } of its pointee.
		if (field.type.isPointer) {
			valueType = bufferPointer(blockStructFor(valueType, field.type.isPacked));
		}
		if (field.type.arrayLength) {
			for (auto inner = field.type.innerArrayLengths.rbegin(); inner != field.type.innerArrayLengths.rend(); ++inner) {
				valueType = array(valueType, *inner);
			}
			valueType = array(valueType, *field.type.arrayLength);
		}
		outTypes.push_back(valueType);

		offset = (offset + layout.alignment - 1) / layout.alignment * layout.alignment;
		if (offset + layout.size > UINT32_MAX) {
			throw CompileError("struct \"" + decl->name + "\" is larger than 4 GiB, which a buffer offset cannot hold");
		}
		outOffsets.push_back(static_cast<uint32_t>(offset));
		offset += layout.size;

		// The struct's alignment is the largest among its members, and its size is
		// the total rounded up to that, so an array of it steps by a whole number
		// of alignments.
		alignment = std::max(alignment, layout.alignment);
	}

	offset = (offset + alignment - 1) / alignment * alignment;
	if (offset > UINT32_MAX) {
		throw CompileError("struct \"" + decl->name + "\" is larger than 4 GiB, which a buffer offset cannot hold");
	}
	outSize = static_cast<uint32_t>(offset);
	return true;
}

// A struct as the element of an array of it: the same members with their
// offsets, and no Block decoration. Vulkan requires a struct nested inside a
// Block to be laid out, and rejects a Block-decorated struct inside an array, so
// this is a third form between the value struct and the buffer's block.
Id TypeTable::arrayElementStruct(const std::string& name) {
	const auto cached = _elementStructs.find(name);
	if (cached != _elementStructs.end()) {
		return cached->second;
	}

	std::vector<Id> fieldTypes;
	std::vector<uint32_t> offsets;
	uint32_t size = 0;
	if (!structMembersFor(name, fieldTypes, offsets, size)) {
		return InvalidId;
	}

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, storageTypesFor(name, fieldTypes));
	for (size_t i = 0; i < fieldTypes.size(); ++i) {
		_builder.emit(spirv::OpMemberDecorate, { id, static_cast<uint32_t>(i),
			static_cast<uint32_t>(spirv::Decoration::Offset), offsets[i] });
		decorateMatrixMember(id, static_cast<uint32_t>(i), fieldTypes[i]);
	}

	_elementStructs.emplace(name, id);
	_structSizes.emplace(id, size);
	return id;
}

Id TypeTable::valueStruct(const std::string& name) {
	const auto cached = _valueStructs.find(name);
	if (cached != _valueStructs.end()) {
		if (cached->second == InvalidId) {
			throw CompileError("recursive value struct \"" + name + "\" is not supported");
		}
		return cached->second;
	}
	const StructDecl* decl = _unit.findStruct(name);
	if (!decl) {
		return InvalidId;
	}
	_valueStructs.emplace(name, InvalidId);
	std::vector<Id> fieldTypes;
	for (const StructField& field: decl->fields) {
		if (field.type.isPointer) {
			throw CompileError("a pointer field in value struct \"" + name + "\" is not lowered yet");
		}
		Id fieldType = !field.type.namedType.empty() ? valueStruct(field.type.namedType)
			: field.type.isMatrix() ? matrix(field.type.scalar, field.type.matrixColumns, field.type.vectorWidth)
			: field.type.vectorWidth > 1 ? vector(field.type.scalar, field.type.vectorWidth)
			: scalar(field.type.scalar);
		if (fieldType == InvalidId) {
			throw CompileError("undeclared type \"" + field.type.namedType + "\"");
		}
		if (field.type.arrayLength) {
			for (auto inner = field.type.innerArrayLengths.rbegin(); inner != field.type.innerArrayLengths.rend(); ++inner) {
				fieldType = array(fieldType, *inner);
			}
			fieldType = array(fieldType, *field.type.arrayLength);
		}
		fieldTypes.push_back(fieldType);
	}

	// A value of a struct with no members is not representable to Mesa's SPIR-V
	// front end (a parameter, a call argument, a constructed value), so a value
	// struct keeps one placeholder member no source member names. The buffer
	// forms keep the empty struct, so layout and reflection are unchanged.
	if (fieldTypes.empty()) {
		fieldTypes.push_back(scalar(ScalarKind::UInt));
	}
	const Id id = _builder.emitDecl(spirv::OpTypeStruct, fieldTypes);

	// Value structs need no buffer layout decorations.
	_valueStructs[name] = id;
	return id;
}

Id TypeTable::namedStruct(const std::string& name) {
	const auto cached = _structs.find(name);
	if (cached != _structs.end()) {
		return cached->second;
	}

	std::vector<Id> fieldTypes;
	std::vector<uint32_t> offsets;
	uint32_t size = 0;
	if (!structMembersFor(name, fieldTypes, offsets, size)) {
		return InvalidId;
	}

	const Id id = _builder.emitDecl(spirv::OpTypeStruct, storageTypesFor(name, fieldTypes));

	// A struct reached through a buffer binding is an interface block, so it
	// needs Block and per-member Offset. std140 rules apply, which for the
	// members the corpus has is Metal's own layout.
	_builder.emit(spirv::OpDecorate, { id, static_cast<uint32_t>(spirv::Decoration::Block) });
	for (size_t i = 0; i < fieldTypes.size(); ++i) {
		_builder.emit(spirv::OpMemberDecorate, { id, static_cast<uint32_t>(i),
			static_cast<uint32_t>(spirv::Decoration::Offset), offsets[i] });
		decorateMatrixMember(id, static_cast<uint32_t>(i), fieldTypes[i]);
	}

	_structs.emplace(name, id);
	return id;
}

void TypeTable::requireCapabilitiesFor(const Type& type) {
	// Float16 and Int64 are emitted where the type is declared, so there is
	// nothing to do here. Kept as a hook rather than removed, because a type
	// needing a capability beyond Shader will need a hook.
	(void)type;
}

//
// Address spaces
//

std::optional<spirv::StorageClassValue> storageClassForAddressSpace(AddressSpace space) {
	switch (space) {
		case AddressSpace::Device: return spirv::StorageClass::StorageBuffer;
		case AddressSpace::Constant: return spirv::StorageClass::Uniform;
		case AddressSpace::Threadgroup: return spirv::StorageClass::Workgroup;
		case AddressSpace::Thread: return spirv::StorageClass::Function;
		case AddressSpace::None: return std::nullopt;
	}

	return std::nullopt;
}

std::vector<const FunctionDecl*> selectEntryPoints(const TranslationUnit& unit, Stage requested) {
	std::vector<const FunctionDecl*> found;

	for (const FunctionDecl& function: unit.functions) {
		// A requested stage narrows the module to that stage, and every function
		// of it is an entry point: Metal's newLibraryWithSource: compiles a whole
		// file, so one .metal source carrying a vertex and a fragment function is
		// one module with two entry points rather than a choice between them.
		if (function.stage == Stage::None) {
			continue;
		}

		if (requested != Stage::None && function.stage != requested) {
			continue;
		}

		found.push_back(&function);
	}

	if (!found.empty()) {
		// Two entry points of one module cannot share a name: Vulkan's validation
		// rule is that entry points are identified by name within an execution
		// model, and a module with two of the same is rejected with "Entry points
		// cannot share the same name and ExecutionMode". This is reachable now
		// that a module may carry several kernels, since a header holding a
		// kernel and included twice declares it twice.
		std::set<std::string> names;
		for (const FunctionDecl* function: found) {
			if (!names.insert(function->name).second) {
				throw CompileError("two entry points are named \"" + function->name
					+ "\"; a module may have several kernels, but entry points are "
					"identified by name within one execution model, so the names must differ");
			}
		}

		// A kernel's workgroup size is OpExecutionModeId LocalSizeId over three
		// spec constants at SpecId 0, 1, 2, and indium writes constantID 0, 1, 2
		// once per pipeline against the module it was handed, so there is one
		// set of ids per module. That is why a module carries one size, and it
		// is also why it can carry several kernels: they all read the same
		// three constants, and [[threads_per_threadgroup]] reads that same size
		// rather than naming one of its own.
		//
		// A second set of spec constants is still not emitted: two decorated
		// constants per SpecId validate and no driver can resolve them.
		return found;
	}

	// A source with no entry point at all is a library with no functions, which the caller handles.
	throw CompileError(std::string("no ") + (requested == Stage::Kernel ? "kernel "
		: requested == Stage::Vertex ? "vertex " : requested == Stage::Fragment ? "fragment " : "")
		+ "entry point in this source");
}

} // namespace mslc
