#include "sema/emitter.h"

namespace mslc {

// The MSL spelling of a builtin, for a diagnostic that names it.
const char* builtinName(ParameterAttributes::Builtin builtin) {
	switch (builtin) {
		case ParameterAttributes::Builtin::ThreadPositionInGrid: return "thread_position_in_grid";
		case ParameterAttributes::Builtin::ThreadgroupPositionInGrid:
			return "threadgroup_position_in_grid";
		case ParameterAttributes::Builtin::ThreadPositionInThreadgroup:
			return "thread_position_in_threadgroup";
		case ParameterAttributes::Builtin::ThreadIndexInThreadgroup:
			return "thread_index_in_threadgroup";
		case ParameterAttributes::Builtin::ThreadsPerThreadgroup:
			return "threads_per_threadgroup";
		case ParameterAttributes::Builtin::VertexID: return "vertex_id";
		case ParameterAttributes::Builtin::InstanceID: return "instance_id";
		case ParameterAttributes::Builtin::BaseVertex: return "base_vertex";
		case ParameterAttributes::Builtin::BaseInstance: return "base_instance";
		case ParameterAttributes::Builtin::Position: return "position";
		case ParameterAttributes::Builtin::FragCoord: return "frag_coord";
		case ParameterAttributes::Builtin::FrontFacing: return "front_facing";
		case ParameterAttributes::Builtin::None: break;
	}

	return "unknown";
}

// Whether Vulkan allows this builtin in this execution model. The dispatch
// ids are compute only and frag_coord is fragment only, and a module that
// used one in the wrong stage is rejected by validation rather than being a
// shader that quietly computes the wrong thing.
bool builtinAllowedInStage(ParameterAttributes::Builtin builtin, Stage stage) {
	switch (builtin) {
		case ParameterAttributes::Builtin::ThreadPositionInGrid:
		case ParameterAttributes::Builtin::ThreadgroupPositionInGrid:
		case ParameterAttributes::Builtin::ThreadPositionInThreadgroup:
		case ParameterAttributes::Builtin::ThreadIndexInThreadgroup:
		case ParameterAttributes::Builtin::ThreadsPerThreadgroup:
			return stage == Stage::Kernel;
		case ParameterAttributes::Builtin::VertexID:
		case ParameterAttributes::Builtin::InstanceID:
		case ParameterAttributes::Builtin::BaseVertex:
		case ParameterAttributes::Builtin::BaseInstance:
			return stage == Stage::Vertex;
		case ParameterAttributes::Builtin::FragCoord:
		case ParameterAttributes::Builtin::FrontFacing:
		case ParameterAttributes::Builtin::Position:
			return stage == Stage::Fragment;
		case ParameterAttributes::Builtin::None: break;
	}

	return false;
}

// A parameter taken by value (a stage input, a builtin) lives in the thread
// address space, which is also what it gets with none written. Apple rejects
// any other, by name, so "device In in [[stage_in]]" is a mistake to report
// rather than a spelling to accept.
void requireThreadAddressSpace(const Parameter& parameter, const char* what) {
	if (parameter.type.addressSpace != AddressSpace::None
		&& parameter.type.addressSpace != AddressSpace::Thread) {
		throw CompileError("parameter \"" + parameter.name + "\" takes " + what + " by value, so it "
			"cannot be in the " + addressSpaceName(parameter.type.addressSpace) + " address space");
	}
}

// MSL does not require [[buffer(n)]] on an entry point's device or
// constant parameters: an unbinding parameter takes the lowest free buffer
// index (declareParameters). add.metal relies on this, since none of its
// pointers carry an attribute.
std::vector<const Parameter*> assignImplicitBindings(const FunctionDecl& entryPoint) {
	std::vector<const Parameter*> bound;

	for (const Parameter& parameter: entryPoint.parameters) {
		if (parameter.attributes.bufferIndex || parameter.attributes.textureIndex
			|| parameter.attributes.samplerIndex || parameter.attributes.builtin
			|| parameter.attributes.stageIn || parameter.attributes.colorIndex
			|| parameter.attributes.threadgroupIndex
			|| parameter.type.addressSpace == AddressSpace::Threadgroup
			|| parameter.type.resource != ResourceKind::None) {
			continue;
		}

		bound.push_back(&parameter);
	}

	return bound;
}

// Binds every entry point parameter: a builtin or resource attribute gets
// its own interface variable, and a buffer parameter is folded into a
// member of the binding-0 address block once every one of them is seen.
void Emitter::declareParameters() {
	const std::vector<const Parameter*> implicitlyBound = assignImplicitBindings(*_entryPoint);
	std::vector<std::pair<size_t, const Parameter*>> resources;

	// An unattributed buffer takes the lowest index that no [[buffer(n)]] anywhere in the list
	// claims and no earlier unattributed buffer took, not its position among them.
	std::map<const Parameter*, uint32_t> implicitBufferIndices;
	{
		std::set<uint32_t> taken;
		for (const Parameter& parameter: _entryPoint->parameters) {
			if (parameter.attributes.bufferIndex) { taken.insert(*parameter.attributes.bufferIndex); }
		}
		uint32_t next = 0;
		for (const Parameter* parameter: implicitlyBound) {
			while (taken.count(next)) { ++next; }
			if (next > 30) {
				throw CompileError("no 'buffer' resource location is available for parameter \""
					+ parameter->name + "\": every index up to 30 is taken");
			}
			implicitBufferIndices[parameter] = next;
			taken.insert(next);
		}
	}

	// Apple numbers the threadgroup arguments apart from the buffers: an explicit
	// [[threadgroup(n)]] keeps n, and one without takes the lowest free index, in order.
	std::map<const Parameter*, uint32_t> threadgroupIndices;
	{
		std::set<uint32_t> taken;
		for (const Parameter& parameter: _entryPoint->parameters) {
			if (!parameter.attributes.threadgroupIndex) { continue; }
			if (parameter.type.addressSpace != AddressSpace::Threadgroup
				|| !(parameter.type.isPointer || parameter.isReference) || parameter.attributes.bufferIndex) {
				throw CompileError("parameter \"" + parameter.name + "\" has [[threadgroup(n)]], which a "
					"pointer or reference in the threadgroup address space takes, with no other index");
			}
			if (!taken.insert(*parameter.attributes.threadgroupIndex).second) {
				throw CompileError("parameter \"" + parameter.name + "\" reserves threadgroup index "
					+ std::to_string(*parameter.attributes.threadgroupIndex) + ", which another argument "
					"already has");
			}
			threadgroupIndices[&parameter] = *parameter.attributes.threadgroupIndex;
		}
		uint32_t next = 0;
		for (const Parameter& parameter: _entryPoint->parameters) {
			if (parameter.type.addressSpace != AddressSpace::Threadgroup || parameter.attributes.bufferIndex
				|| threadgroupIndices.count(&parameter)) {
				continue;
			}
			if (!(parameter.type.isPointer || parameter.isReference)) {
				const ParameterAttributes& a = parameter.attributes;
				if (a.stageIn || a.builtin || a.colorIndex || a.textureIndex || a.samplerIndex) { continue; }
				throw CompileError("parameter \"" + parameter.name + "\" is a threadgroup value; a parameter in "
					"the threadgroup address space is a pointer or a reference");
			}
			while (taken.count(next)) { ++next; }
			if (next > 30) {
				throw CompileError("parameter \"" + parameter.name + "\" is past the last threadgroup index, 30");
			}
			taken.insert(next);
			threadgroupIndices[&parameter] = next;
		}
	}

	for (size_t index = 0; index < _entryPoint->parameters.size(); ++index) {
		const Parameter& parameter = _entryPoint->parameters[index];
		const ParameterAttributes& attributes = parameter.attributes;

		// A texture or sampler parameter, or an attribute that says it is one.
		// Apple takes [[texture]] on a texture and [[sampler]] on a sampler and
		// nothing else: "type 'device float *' is not valid for attribute
		// 'texture'".
		const bool isTexture = parameter.type.isTexture();
		const bool isSampler = parameter.type.resource == ResourceKind::Sampler;
		if (isTexture || isSampler || attributes.textureIndex || attributes.samplerIndex) {
			const char* expected = isTexture ? "texture" : "sampler";
			if ((attributes.textureIndex && !isTexture) || (attributes.samplerIndex && !isSampler)
				|| attributes.bufferIndex || attributes.builtin || attributes.stageIn) {
				throw CompileError("parameter \"" + parameter.name + "\" is a " + typeName(parameter.type)
					+ ", and its attribute is not valid for it"
					+ (isTexture || isSampler ? std::string("; it takes [[") + expected + "(n)]] only" : ""));
			}

			resources.emplace_back(index, &parameter);
			continue;
		}

		if (parameter.attributes.stageIn) {
			if (_entryPoint->stage == Stage::Kernel) {
				throw CompileError("[[stage_in]] on a kernel function is not lowered (parameter \""
					+ parameter.name + "\"); mslc lowers it on a vertex or a fragment function");
			}

			if (_stageIn) {
				throw CompileError("a function takes one [[stage_in]] parameter, and \""
					+ _entryPoint->name + "\" has a second (\"" + parameter.name + "\")");
			}

			if (parameter.isReference) {
				throw CompileError("[[stage_in]] parameter \"" + parameter.name + "\" is a "
					"reference, which Apple's compiler does not allow; take the struct by value");
			}

			requireThreadAddressSpace(parameter, "[[stage_in]]");

			const StructDecl* decl = structValue(parameter.type);
			if (!decl) {
				throw CompileError("[[stage_in]] parameter \"" + parameter.name + "\" is a "
					+ typeName(parameter.type) + "; mslc lowers a [[stage_in]] struct and no "
						"other type");
			}

			_stageIn = &parameter;
			_stageInputs = _entryPoint->stage == Stage::Vertex
				? declareVertexAttributes(*decl)
				: declareStageStruct(*decl, spirv::StorageClass::Input);
			continue;
		}

		if (parameter.attributes.colorIndex) {
			if (_entryPoint->stage != Stage::Fragment) {
				throw CompileError("parameter \"" + parameter.name + "\" is [[color(n)]], which only a "
					"fragment function reads");
			}
			if (parameter.isReference || parameter.type.isPointer) {
				throw CompileError("parameter \"" + parameter.name + "\" is [[color(n)]], which Apple's "
					"compiler takes by value");
			}
			requireThreadAddressSpace(parameter, "[[color(n)]]");
			_attachmentParameters.push_back({ &parameter, declareFramebufferFetch(parameter.type,
				*parameter.attributes.colorIndex, parameter.name, "parameter \"" + parameter.name + "\"") });
			continue;
		}

		if (parameter.attributes.builtin) {
			requireThreadAddressSpace(parameter, "a builtin");

			if (*parameter.attributes.builtin == ParameterAttributes::Builtin::ThreadsPerThreadgroup) {
				if (_entryPoint->stage != Stage::Kernel) {
					throw CompileError("builtin \"threads_per_threadgroup\" is not available in a "
						+ std::string(_entryPoint->stage == Stage::Vertex ? "vertex" : "fragment")
						+ " function (parameter \"" + parameter.name + "\")");
				}

				const Type& type = parameter.type;
				const bool width = type.vectorWidth <= 3;
				const bool unsignedInt = type.scalar == ScalarKind::UInt || type.scalar == ScalarKind::UShort;
				if (!width || !unsignedInt || type.isPointer || type.isMatrix() || type.arrayLength
					|| !type.namedType.empty() || type.isPacked || parameter.isReference) {
					throw CompileError("parameter \"" + parameter.name + "\" is a " + typeName(type)
						+ ", which is not valid for [[threads_per_threadgroup]]; it takes uint or ushort, "
							"or a vector of two or three of them");
				}

				if (_threadsPerThreadgroup) {
					throw CompileError("[[threads_per_threadgroup]] is on both \""
						+ _threadsPerThreadgroup->name + "\" and \"" + parameter.name
						+ "\"; a function takes it once");
				}

				_threadsPerThreadgroup = &parameter;
				continue;
			}
			const spirv::BuiltInValue spvBuiltin = [&]() {
				switch (*parameter.attributes.builtin) {
					case ParameterAttributes::Builtin::ThreadPositionInGrid:
						return spirv::BuiltIn::GlobalInvocationId;
					case ParameterAttributes::Builtin::ThreadPositionInThreadgroup:
						return spirv::BuiltIn::LocalInvocationId;
					// The linear index within the workgroup, which is what
					// Metal names, rather than a position within it.
					case ParameterAttributes::Builtin::ThreadIndexInThreadgroup:
						return spirv::BuiltIn::LocalInvocationIndex;
					case ParameterAttributes::Builtin::ThreadgroupPositionInGrid:
						return spirv::BuiltIn::WorkgroupId;
					// Vulkan forbids VertexId and InstanceId. The Index forms
					// include the base vertex and instance, as Metal's do.
					case ParameterAttributes::Builtin::VertexID:
						return spirv::BuiltIn::VertexIndex;
					case ParameterAttributes::Builtin::InstanceID:
						return spirv::BuiltIn::InstanceIndex;
					// An indexed draw's vertexOffset and a draw's firstInstance,
					// which are Metal's base vertex and base instance when the
					// runtime passes them through.
					case ParameterAttributes::Builtin::BaseVertex:
						return spirv::BuiltIn::BaseVertex;
					case ParameterAttributes::Builtin::BaseInstance:
						return spirv::BuiltIn::BaseInstance;
					case ParameterAttributes::Builtin::FragCoord:
					case ParameterAttributes::Builtin::Position:
						return spirv::BuiltIn::FragCoord;
					case ParameterAttributes::Builtin::FrontFacing:
						return spirv::BuiltIn::FrontFacing;
					default:
						throw CompileError("this builtin is recognised but not lowered yet");
				}
			}();

			// Vulkan fixes each builtin's type. The compute ids are always
			// uvec3 even where MSL declares a narrower width, so a scalar
			// declaration extracts a component.
			Id typeId = InvalidId;
			switch (spvBuiltin) {
				case spirv::BuiltIn::VertexIndex:
				case spirv::BuiltIn::InstanceIndex:
				case spirv::BuiltIn::BaseVertex:
				case spirv::BuiltIn::BaseInstance:
					typeId = _types.scalar(ScalarKind::UInt);
					break;
				// A scalar, unlike the compute ids above: LocalInvocationIndex
				// is a linear index within the workgroup, so there is no
				// component to extract and declaring it uvec3 would be wrong.
				case spirv::BuiltIn::LocalInvocationIndex:
					typeId = _types.scalar(ScalarKind::UInt);
					break;
				case spirv::BuiltIn::FragCoord:
					typeId = _types.vector(ScalarKind::Float, 4);
					break;
				case spirv::BuiltIn::FrontFacing:
					typeId = _boolType;
					break;
				default:
					typeId = _types.vector(ScalarKind::UInt, 3);
					break;
			}
			_builder.setSection(spirv::Section::TypesGlobals);
			const Id pointerType = _types.pointer(spirv::StorageClass::Input, typeId);
			const Id id = _builder.emitDeclTyped(spirv::OpVariable, pointerType,
				{ static_cast<uint32_t>(spirv::StorageClass::Input) });

			// Vulkan fixes which execution models each builtin is legal in, and
			// an integer Input on a fragment entry point additionally has to be
			// Flat. A builtin used in the wrong stage is rejected rather than
			// decorated: the alternative is a module that validates on one
			// stage's terms and is wrong on another's. Checked before the
			// decoration, so a rejected builtin is never emitted.
			if (!builtinAllowedInStage(*parameter.attributes.builtin, _entryPoint->stage)) {
				throw CompileError(std::string("builtin \"") + builtinName(*parameter.attributes.builtin)
					+ "\" is not available in a "
					+ (_entryPoint->stage == Stage::Kernel ? "kernel"
						: _entryPoint->stage == Stage::Vertex ? "vertex" : "fragment")
					+ " function (parameter \"" + parameter.name + "\")");
			}

			// A direct FragCoord-mapped parameter (either MSL spelling) has to
			// declare a float4: nothing else reinterprets the builtin's fixed
			// Vulkan type, so a narrower or wider declaration would silently
			// alias or misread it. At most one FragCoord source (this
			// parameter, another parameter, or a stage-struct [[position]]
			// field) may exist per fragment entry point, since Vulkan forbids
			// two Input variables decorated with the same BuiltIn.
			if (spvBuiltin == spirv::BuiltIn::FragCoord) {
				if (declaredTypeOf(parameter.type) != _types.vector(ScalarKind::Float, 4)) {
					throw CompileError("parameter \"" + parameter.name + "\" is [[" +
						builtinName(*parameter.attributes.builtin) + "]], which has to be a float4");
				}
				if (_hasFragCoordInput) {
					throw CompileError("parameter \"" + parameter.name + "\" is [[" +
						builtinName(*parameter.attributes.builtin) + "]], which collides with this "
						"function's other fragment position input");
				}
				_hasFragCoordInput = true;
			}

			// Apple takes a compute id as uint or ushort, scalar or up to three
			// wide, and rejects int, float and a fourth lane.
			const bool computeId = spvBuiltin == spirv::BuiltIn::GlobalInvocationId
				|| spvBuiltin == spirv::BuiltIn::LocalInvocationId
				|| spvBuiltin == spirv::BuiltIn::WorkgroupId;
			if (computeId && (parameter.type.vectorWidth > 3 || parameter.type.vectorWidth == 1
				|| (parameter.type.scalar != ScalarKind::UInt && parameter.type.scalar != ScalarKind::UShort))) {
				throw CompileError("parameter \"" + parameter.name + "\" is [[" +
					builtinName(*parameter.attributes.builtin) + "]], which has to be a uint or ushort "
					"scalar or a vector of two or three of them");
			}

			// LocalInvocationIndex is the odd one out: it is a linear index
			// rather than a position, so Apple takes it as a uint or ushort
			// SCALAR and rejects every vector, including uint2 and uint3. Left
			// to the compute-id rule above it would over-accept float, int, bool
			// and half, and a uint3 would fall through to the swizzle check and
			// be reported as ".x swizzles a value that is not a vector", which
			// names neither the attribute nor the real problem.
			//
			// A scalar is vectorWidth 0, not 1: the compute-id rule above reads
			// "== 1" as the scalar to reject.
			if (spvBuiltin == spirv::BuiltIn::LocalInvocationIndex
				&& (parameter.type.vectorWidth > 0
					|| (parameter.type.scalar != ScalarKind::UInt
						&& parameter.type.scalar != ScalarKind::UShort))) {
				throw CompileError("parameter \"" + parameter.name + "\" is [[" +
					builtinName(*parameter.attributes.builtin) + "]], which has to be a uint or ushort "
					"scalar");
			}

			// Apple takes the base vertex and base instance as a uint or ushort scalar by value, and
			// reports a reference, a pointer or an atomic.
			const bool drawParameter = spvBuiltin == spirv::BuiltIn::BaseVertex
				|| spvBuiltin == spirv::BuiltIn::BaseInstance;
			if (drawParameter) {
				if (parameter.type.vectorWidth > 0 || parameter.type.isMatrix() || parameter.type.arrayLength
					|| !parameter.type.namedType.empty() || parameter.isReference || parameter.type.isPointer
					|| parameter.type.isAtomic
					|| (parameter.type.scalar != ScalarKind::UInt && parameter.type.scalar != ScalarKind::UShort)) {
					throw CompileError("parameter \"" + parameter.name + "\" is [[" +
						builtinName(*parameter.attributes.builtin) + "]], which has to be a uint or ushort "
						"scalar");
				}
				// Core since SPIR-V 1.3. Vulkan also needs the shaderDrawParameters feature.
				_types.requireCapability(spirv::Capability::DrawParameters);
			}

			_builder.setSection(spirv::Section::Annotations);
			_builder.emit(spirv::OpDecorate, { id,
				static_cast<uint32_t>(spirv::Decoration::BuiltIn),
				static_cast<uint32_t>(spvBuiltin) });

			// An integer Input on a fragment entry point has to be Flat:
			// there is no sensible way to interpolate an integer, and
			// VUID-StandaloneSpirv-Flat-04744 rejects the module without it.
			// A compute or vertex entry point has no interpolation at all, so
			// the decoration would be meaningless there.
			if (_entryPoint->stage == Stage::Fragment && spvBuiltin != spirv::BuiltIn::FragCoord
				&& spvBuiltin != spirv::BuiltIn::FrontFacing) {
				_builder.emit(spirv::OpDecorate, { id,
					static_cast<uint32_t>(spirv::Decoration::Flat) });
			}

			Binding binding;
			binding.id = id;
			binding.isPointer = true;
			binding.pointeeType = typeId;
			binding.storageClass = spirv::StorageClass::Input;
			const uint32_t declaredLanes = std::max(parameter.type.vectorWidth, 1u);
			if (computeId) {
				binding.leadingLanes = declaredLanes;
				binding.pointeeMsl = parameter.type;
			}
			binding.readOnly = parameter.type.isConst;
			_bindings[parameter.name] = binding;
			_interface.push_back(id);
			if (spvBuiltin == spirv::BuiltIn::GlobalInvocationId) {
				_globalInvocationVariable = id;
			}
			continue;
		}

		if (threadgroupIndices.count(&parameter)) {
			if (_entryPoint->stage != Stage::Kernel) {
				throw CompileError("parameter \"" + parameter.name + "\" is a threadgroup parameter; "
					"threadgroup parameters are only supported on kernel functions");
			}
			declareThreadgroupArgument(parameter, index, threadgroupIndices[&parameter]);
			continue;
		}

		// An unbinding parameter takes the lowest buffer index no attribute claims.
		uint32_t bindingIndex = 0;
		if (parameter.attributes.bufferIndex) {
			bindingIndex = *parameter.attributes.bufferIndex;
		} else {
			const auto it = implicitBufferIndices.find(&parameter);
			if (it == implicitBufferIndices.end()) {
				throw CompileError("parameter \"" + parameter.name + "\" has no [[buffer]], "
					"[[texture]], [[sampler]] or builtin attribute, and no address space mslc "
					"can bind, so there is nothing to bind it to");
			}
			bindingIndex = it->second;
		}

		// A threadgroup pointer with no [[buffer]] is an argument of its own and was
		// handled above; with [[buffer]] Apple rejects it, and so does the check below.
		const AddressSpace space = parameter.type.addressSpace;
		if (space == AddressSpace::None) {
			throw CompileError("parameter \"" + parameter.name + "\": pointer parameter needs an "
				"explicit address space (device or constant)");
		}
		if (space != AddressSpace::Device && space != AddressSpace::Constant) {
			throw CompileError("parameter \"" + parameter.name + "\" needs a device or constant "
				"address space to be a buffer binding, not " + std::string(addressSpaceName(space)));
		}

		Id pointeeType = InvalidId;
		const StructDecl* structType = nullptr;

		if (!parameter.type.namedType.empty()) {
			// A Metal "device T*" is a buffer of T, so the element is the struct
			// laid out for an array rather than the Block-decorated form: Vulkan
			// requires a struct nested inside a Block to be laid out, and rejects a
			// Block-decorated struct inside an array. A struct reached directly is
			// the buffer itself, so it is the Block form.
			if (parameter.type.isPointer) {
				pointeeType = _types.arrayElementStruct(parameter.type.namedType);
			} else {
				pointeeType = _types.namedStruct(parameter.type.namedType);
			}

			if (pointeeType == InvalidId) {
				throw CompileError("parameter \"" + parameter.name + "\" refers to undeclared "
					"type \"" + parameter.type.namedType + "\"");
			}

			structType = _unit.findStruct(parameter.type.namedType);
		} else {
			// A matrix's layout is a MatrixStride, which SPIR-V only allows on a
			// struct member, and a buffer reached directly is not wrapped in one.
			pointeeType = declaredTypeOf(parameter.type);
		}

		// A buffer parameter's address is a member of the binding-0 block rather
		// than a descriptor of its own, so what a member points at is the
		// buffer's own type. A pointer parameter is a whole buffer, and
		// OpAccessChain rejects a non-composite base, so the element type is
		// wrapped in { T runtime_array[] } and the parameter indexes through
		// that -- for a struct element as much as for a scalar one.
		Id bufferPointee = pointeeType;
		// A bool is a byte in a buffer, so the reference's own memory is that byte (or
		// the bytes of a bool vector), mapped back to a bool where it is used.
		const bool matrixReference = !parameter.type.isPointer && parameter.type.isMatrix();
		if (matrixReference) {
			// { matrix m; }: the matrix is member 0, which carries its layout.
			bufferPointee = _types.matrixReferenceStruct(pointeeType);
		}
		const bool boolReference = !parameter.type.isPointer && _types.isBool(pointeeType);
		if (boolReference) {
			bufferPointee = _types.boolStorage(pointeeType);
		}
		bool isBuffer = false;
		if (parameter.type.isPointer) {
			bufferPointee = _types.blockStructFor(pointeeType, parameter.type.isPacked);
			isBuffer = true;
		}

		// The member index is this parameter's position among the buffer
		// parameters in declaration order, which is what indium fills in. It is
		// not the Metal index: that says which buffer the app bound there, and
		// goes in the reflection.
		const auto memberIndex = static_cast<uint32_t>(_bufferMembers.size());
		_bufferMembers.push_back(bufferPointee);
		if (boolReference) {
			_bufferBoolTypes[memberIndex] = pointeeType;
		}
		if (matrixReference) {
			_bufferMatrixTypes[memberIndex] = pointeeType;
		}

		// The block is emitted after the loop, once every member is known.
		Binding binding;
		binding.isPointer = true;
		binding.pointeeType = pointeeType;
		binding.bufferPointeeType = bufferPointee;
		binding.storageClass = spirv::StorageClass::PhysicalStorageBuffer;
		binding.structType = structType;
		binding.isBuffer = isBuffer;
		binding.pointeeMsl = parameter.type;
		binding.pointeeMsl.isPointer = false;
		binding.pointeeMsl.isConst = false;
		binding.readOnly = parameter.type.isConst
			|| parameter.type.addressSpace == AddressSpace::Constant;
		binding.memberIndex = memberIndex;
		_bindings[parameter.name] = binding;

		// The Metal index says which buffer the app bound at that slot. Every
		// buffer of an entry point shares one block at binding 0, so it is not a
		// per-buffer descriptor binding and the reflection says which one instead.
		// The set is the one the block is actually declared in, which is 1 for a
		// fragment entry point.
		//
		// The separator goes before the entry rather than after it, because a
		// trailing comma makes the array a syntax error: json.load refuses
		// "Illegal trailing comma before end of array", so a reflection a reader
		// cannot parse is not a reflection. That is why the first binding is the
		// one that carries no comma.
		addReflectionEntry("{ \"kind\": \"Buffer\", \"metal_index\": "
			+ std::to_string(bindingIndex)
			+ ", \"descriptor\": { \"set\": " + std::to_string(descriptorSet())
			+ ", \"binding\": 0 }"
			+ ", \"member\": " + std::to_string(memberIndex)
			+ ", \"param_index\": " + std::to_string(index)
			+ ", \"name\": \"" + parameter.name + "\" }");
	}

	// One block for every buffer parameter, at binding 0. It is emitted here
	// rather than per parameter because its member list is only known once the
	// loop is done, and indium binds it as a single uniform buffer whose
	// entries are the buffer addresses in this order.
	if (!_bufferMembers.empty()) {
		_addressBlock = _types.addressBlock(_bufferMembers, descriptorSet());
		_interface.push_back(_addressBlock.variable);
	}

	declareResources(resources);
	std::set<std::string> used;
	collectIdentifiers(*_entryPoint->body, used, false);
	reserveLocalSamplers(*_entryPoint->body, used);
	_threadgroupLocals.clear();
	reserveThreadgroupLocals(*_entryPoint->body);
	reserveFileScopeSamplers();
	declareGridMaskInput();
	// Last, so the constexpr samplers keep the bindings right after the textures and samplers,
	// the order indium binds by; an attachment is not part of Iridium's argument list.
	declareAttachments();
}

// A kernel that skips the threads outside the grid reads the thread position in the grid, which has to
// be an Input variable in the entry point's interface, so it is declared with the parameters. A kernel
// that has a barrier anywhere cannot skip threads (a thread that returns before a barrier leaves
// the rest of its workgroup waiting), and neither can one in a module with no mask requested.
void Emitter::declareGridMaskInput() {
	_masksGridSize = false;
	if (!_options.maskGridSize || _entryPoint->stage != Stage::Kernel) {
		return;
	}
	if (callsBarrier(*_entryPoint->body)) {
		return;
	}
	for (const FunctionDecl& helper: _unit.helpers) {
		if (helper.body && callsBarrier(*helper.body)) {
			return;
		}
	}
	_masksGridSize = true;

	if (_globalInvocationVariable == InvalidId) {
		_builder.setSection(spirv::Section::TypesGlobals);
		const Id vectorType = _types.vector(ScalarKind::UInt, 3);
		_globalInvocationVariable = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Input, vectorType),
			{ static_cast<uint32_t>(spirv::StorageClass::Input) });
		_builder.setSection(spirv::Section::Annotations);
		_builder.emit(spirv::OpDecorate, { _globalInvocationVariable,
			static_cast<uint32_t>(spirv::Decoration::BuiltIn),
			static_cast<uint32_t>(spirv::BuiltIn::GlobalInvocationId) });
		_interface.push_back(_globalInvocationVariable);
	}
}

// if (any(position_in_grid >= grid_size)) return; at the start of the kernel.
void Emitter::emitGridMask() {
	if (!_masksGridSize) {
		return;
	}
	const Id vectorType = _types.vector(ScalarKind::UInt, 3);
	const Id position = _builder.emitTyped(spirv::OpLoad, vectorType, { _globalInvocationVariable });
	Id outside = InvalidId;
	for (uint32_t i = 0; i < 3; ++i) {
		const Id component = _builder.emitTyped(spirv::OpCompositeExtract, _uintType, { position, i });
		const Id past = _builder.emitTyped(spirv::OpUGreaterThanEqual, _boolType, { component, _gridSize[i] });
		outside = outside == InvalidId ? past
			: _builder.emitTyped(spirv::OpLogicalOr, _boolType, { outside, past });
	}
	const Id exitLabel = _builder.nextId();
	const Id bodyLabel = _builder.nextId();
	_builder.emit(spirv::OpSelectionMerge, { bodyLabel, kSelectionControlNone });
	terminate(spirv::OpBranchConditional, { outside, exitLabel, bodyLabel });
	beginBlock(exitLabel);
	terminate(spirv::OpReturn, { });
	beginBlock(bodyLabel);
}

// A kernel's `threadgroup T *p [[threadgroup(n)]]`. Metal sets the byte length at encode time
// (setThreadgroupMemoryLength:atIndex:), Vulkan fixes Workgroup size at pipeline creation, so the
// variable is a Workgroup array whose length is a specialization constant, set to the element
// count the host computes from the byte length and the reflected element_size. The SpecId is
// per argument, from 3 up, because SpecIds are per module and two kernels can take index n with
// different element types; each entry point's reflection names its own.
void Emitter::declareThreadgroupArgument(const Parameter& parameter, size_t index, uint32_t metalIndex) {
	const std::string what = "threadgroup argument \"" + parameter.name + "\"";
	if (parameter.isReference) {
		throw CompileError(what + " is a reference, which is not lowered yet; take a pointer");
	}
	Type element = parameter.type;
	element.isPointer = false;
	if (element.isAtomic) {
		throw CompileError(what + " points at an atomic, which is not lowered yet");
	}
	if (element.resource != ResourceKind::None || element.arrayLength
		|| (element.scalar == ScalarKind::Void && element.namedType.empty())) {
		throw CompileError(what + " has a pointee mslc cannot lay out in threadgroup memory; it takes a "
			"scalar, vector, matrix or struct");
	}
	const uint64_t elementSize = sizeofType(element, _unit);

	const Id elementType = declaredTypeOf(element);
	const Id count = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType, { 1u });
	if (_nextSpecId == kFunctionConstantSpecializationID) {
		throw CompileError(what + " is one more than mslc has specialization constants for");
	}
	const uint32_t specId = _nextSpecId++;
	_builder.emit(spirv::OpDecorate, { count, static_cast<uint32_t>(spirv::Decoration::SpecId), specId });
	const Id arrayType = _types.arrayOfLengthId(elementType, count);
	const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
		_types.pointer(spirv::StorageClass::Workgroup, arrayType),
		{ static_cast<uint32_t>(spirv::StorageClass::Workgroup) });
	_interface.push_back(variable);

	// An array with no length of its own to check literal indexes against.
	Type msl = element;
	msl.arrayLength = UINT32_MAX;
	bindLocal(parameter.name, variable, arrayType, msl, spirv::StorageClass::Workgroup);

	addReflectionEntry("{ \"kind\": \"ThreadgroupMemory\", \"metal_index\": " + std::to_string(metalIndex)
		+ ", \"spec_id\": " + std::to_string(specId)
		+ ", \"element_size\": " + std::to_string(elementSize)
		+ ", \"param_index\": " + std::to_string(index)
		+ ", \"name\": \"" + parameter.name + "\" }");
}

} // namespace mslc
