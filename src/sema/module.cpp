#include "sema/emitter.h"

namespace mslc {

// Emits one entry point: its parameters, its OpEntryPoint, its execution
// modes and its function. Everything stage-dependent is decided from
// _entryPoint, which the caller points at the one being emitted. functionType
// is the module's one void function type.
void Emitter::emitEntryPoint(Id functionType) {
	declareParameters();
	declareStageOutputs();

	spirv::ExecutionModelValue model = spirv::ExecutionModel::GLCompute;
	if (_entryPoint->stage == Stage::Vertex) {
		model = spirv::ExecutionModel::Vertex;
	} else if (_entryPoint->stage == Stage::Fragment) {
		model = spirv::ExecutionModel::Fragment;
	}

	// The entry point's own id has to exist before it is named in
	// OpEntryPoint, so it is allocated here rather than after.
	_entryPointId = _builder.nextId();

	_builder.setSection(spirv::Section::EntryPoints);
	{
		std::vector<uint32_t> operands = {
			static_cast<uint32_t>(model), _entryPointId
		};
		spirv::Builder::appendString(operands, _entryPoint->name);
		for (Id id: _interface) {
			operands.push_back(id);
		}
		_builder.emit(spirv::OpEntryPoint, operands);
	}

	// mslc models Metal's safe math (__METAL_FAST_MATH__ is 0), so signed zeros,
	// infinities and NaNs must survive. Without this a NIR driver folds x * 0 and
	// x + (-x). The capability is core from SPIR-V 1.4; Vulkan needs
	// shaderSignedZeroInfNanPreserveFloat32. The 16-bit mode is added in run()
	// once it is known whether the module uses half.
	if (!_floatControlsDeclared) {
		_builder.emitNoResult(spirv::OpCapability,
			{ static_cast<uint32_t>(spirv::Capability::SignedZeroInfNanPreserve) });
		_floatControlsDeclared = true;
	}
	_builder.setSection(spirv::Section::ExecutionModes);
	_builder.emitNoResult(spirv::OpExecutionMode, { _entryPointId,
		static_cast<uint32_t>(spirv::ExecutionMode::SignedZeroInfNanPreserve), 32u });
	_entryPointIds.push_back(_entryPointId);
	if (_entryPoint->stage == Stage::Kernel) {
		// The workgroup size is a spec constant rather than a literal, so
		// indium can supply Metal's threadsPerThreadgroup through
		// VkSpecializationInfo at pipeline creation. A literal would be baked
		// in and the value the app asked for would be ignored.
		//
		// One set of SpecIds for the module, not one per entry point. indium
		// writes constantID 0, 1, 2 once per pipeline against the module it was
		// given, so a second compute entry point declaring its own 0, 1, 2 would
		// leave two decorated constants per id, which spirv-val accepts and no
		// driver can resolve: both entry points would read whichever the driver
		// picked.
		//
		// Created once and reused, so a module with several kernels still has
		// exactly three SpecId decorations and every LocalSizeId names the same
		// three ids. Emitting a second set is not a validation error, which is
		// why this has to be checked here and not by the suite alone.
		if (_localSize[0] == InvalidId) {
			const Id x = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType,
				{ _options.localSizeX });
			const Id y = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType,
				{ _options.localSizeY });
			const Id z = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType,
				{ _options.localSizeZ });
			_localSize[0] = x;
			_localSize[1] = y;
			_localSize[2] = z;
			_builder.emit(spirv::OpDecorate, { x,
				static_cast<uint32_t>(spirv::Decoration::SpecId), 0u });
			_builder.emit(spirv::OpDecorate, { y,
				static_cast<uint32_t>(spirv::Decoration::SpecId), 1u });
			_builder.emit(spirv::OpDecorate, { z,
				static_cast<uint32_t>(spirv::Decoration::SpecId), 2u });
		}

		// The grid size of the threads a masked kernel keeps, also one set for the module. Their default is
		// the largest value, which keeps every thread, so a dispatch that does not know about them is not
		// changed. The ids are far from the ones a function constant takes (indium numbers those from 3).
		if (_masksGridSize && _gridSize[0] == InvalidId) {
			for (uint32_t i = 0; i < 3; ++i) {
				_gridSize[i] = _builder.emitDeclTyped(spirv::OpSpecConstant, _uintType, { 0xFFFFFFFFu });
				_builder.emit(spirv::OpDecorate, { _gridSize[i],
					static_cast<uint32_t>(spirv::Decoration::SpecId), kGridSizeSpecializationID + i });
			}
		}

		// OpExecutionModeId, not OpExecutionMode: a mode whose extra operands
		// are ids has its own instruction, and OpExecutionMode rejects id
		// operands outright.
		//
		// The section is set again here because the decorations above moved it
		// to the annotations, and an execution mode emitted from there lands
		// in the wrong section and is ignored.
		_builder.setSection(spirv::Section::ExecutionModes);
		_builder.emit(spirv::OpExecutionModeId, { _entryPointId,
			static_cast<uint32_t>(spirv::ExecutionMode::LocalSizeId),
			_localSize[0], _localSize[1], _localSize[2] });
	}

	if (_entryPoint->stage == Stage::Fragment) {
		// Vulkan requires one of the two origin modes on a fragment entry
		// point, and Metal's framebuffer origin is upper left.
		_builder.setSection(spirv::Section::ExecutionModes);
		_builder.emit(spirv::OpExecutionMode, { _entryPointId,
			static_cast<uint32_t>(spirv::ExecutionMode::OriginUpperLeft) });
		if (const StructDecl* decl = structValue(_entryPoint->returnType)) {
			for (const StructField& field: decl->fields) {
				if (field.attributes.stencil) {
					_builder.emit(spirv::OpExecutionMode, { _entryPointId,
						static_cast<uint32_t>(spirv::ExecutionMode::StencilRefReplacingEXT) });
					continue;
				}
				if (!field.attributes.depthMode) {
					continue;
				}
				_builder.emit(spirv::OpExecutionMode, { _entryPointId,
					static_cast<uint32_t>(spirv::ExecutionMode::DepthReplacing) });
				if (*field.attributes.depthMode != DepthMode::Any) {
					_builder.emit(spirv::OpExecutionMode, { _entryPointId,
						static_cast<uint32_t>(*field.attributes.depthMode == DepthMode::Less
							? spirv::ExecutionMode::DepthLess : spirv::ExecutionMode::DepthGreater) });
				}
			}
		}
	}

	_builder.setSection(spirv::Section::Functions);
	// The function's own id has to be the one the entry point names, so it
	// is written explicitly rather than taken from the emit's return.
	_builder.emitDeclTypedAt(spirv::OpFunction, _voidType, _entryPointId,
		{ kFunctionControlNone, functionType });
	beginBlock(_builder.nextId());
	preloadBufferBases();
	loadStageInputs();
	loadThreadsPerThreadgroup();
	copyWrittenBuiltins();
	emitGridMask();
	emitFunctionBody(*_entryPoint->body);
	if (!_terminated) {
		terminate(spirv::OpReturn, { });
	}
	_builder.emit(spirv::OpFunctionEnd, { });

	// The helpers this entry point called, and the ones those called.
	for (size_t i = 0; i < _helperQueue.size(); ++i) {
		emitHelper(*_helperQueue[i]);
	}
	_helperQueue.clear();
}

void Emitter::linkageAttributes(Id function, const std::string& name, uint32_t type) {
	std::vector<uint32_t> operands { function, static_cast<uint32_t>(spirv::Decoration::LinkageAttributes) };
	spirv::Builder::appendString(operands, name);
	operands.push_back(type);
	_builder.emit(spirv::OpDecorate, operands);
}

// Declarations come before every definition in a module's function section, so the
// helpers this unit calls and does not define are emitted before the first body.
void Emitter::emitImports() {
	_entryPoint = &_libraryScope;
	// What the unit emits as a body: its entry points, what it exports, and what
	// those call. A helper nothing reaches is not emitted, so it imports nothing.
	std::set<const FunctionDecl*> seen;
	std::vector<const FunctionDecl*> pending = _entryPoints;
	for (const FunctionDecl& helper: _unit.helpers) {
		// A member is reached through a receiver this scan cannot resolve, so every
		// member body counts as emitted: an import it needs must come before any body.
		if (isExported(helper) || (helper.body && isMemberName(helper.name))) {
			pending.push_back(&helper);
		}
	}
	std::vector<HelperFunction*> imports;
	while (!pending.empty()) {
		const FunctionDecl* function = pending.back();
		pending.pop_back();
		if (!seen.insert(function).second) {
			continue;
		}
		std::vector<const Expression*> calls;
		collectCalls(*function->body, calls);
		for (const Expression* call: calls) {
			if (call->left->kind != ExpressionKind::Identifier) {
				continue;
			}
			const std::vector<HelperFunction*> called = helpersCalled(*call);
			for (HelperFunction* found: called) {
				if (found->definition->body) {
					pending.push_back(found->definition);
				} else if (called.size() > 1) {
					throw CompileError("\"" + found->definition->name + "(" + parameterSignature(*found->definition)
						+ ")\" is declared but defined elsewhere, and a call to it is chosen among overloads; "
						"importing one overload of a name is not lowered yet");
				} else {
					imports.push_back(found);
				}
			}
		}
	}
	for (HelperFunction* entry: imports) {
		if (std::any_of(entry->definition->parameters.begin(), entry->definition->parameters.end(),
			[](const Parameter& parameter) { return parameter.type.isPointer; })) {
			throw CompileError("\"" + entry->definition->name + "\" is declared but defined elsewhere, and "
				"takes a pointer parameter; a thread or threadgroup pointer is lowered per caller, so it cannot be a "
				"link-time symbol");
		}
		helperFunctionId(*entry);
	}
	for (size_t i = 0; i < _helperQueue.size(); ++i) {
		emitHelper(*_helperQueue[i]);
	}
	_helperQueue.clear();
}

bool Emitter::isExported(const FunctionDecl& helper) const {
	return helper.body && _options.exportOnly && _options.exportOnly->count(helper.name);
}

// A Linkage object exports each helper it defines that lowers without a stage, whether
// or not anything in the unit calls it: another unit's call is what makes it live.
void Emitter::emitLibraryHelpers() {
	_entryPoint = &_libraryScope;
	for (const FunctionDecl& helper: _unit.helpers) {
		if (!isExported(helper)) {
			continue;
		}
		if (std::any_of(helper.parameters.begin(), helper.parameters.end(),
			[](const Parameter& parameter) { return parameter.type.isPointer; })) {
			throw CompileError("helper \"" + helper.name + "\" takes a pointer parameter, which is lowered "
				"per caller, so it cannot be exported from a library");
		}
		try {
			linkageAttributes(helperFunctionId(_helpers.at(helperKey(helper, _overloadedNames))),
				linkageName(helper), kLinkageExport);
			for (size_t i = 0; i < _helperQueue.size(); ++i) {
				emitHelper(*_helperQueue[i]);
			}
			_helperQueue.clear();
		} catch (const StageError& error) {
			throw CompileError("helper \"" + helper.name + "\" cannot be exported from a library, which does "
				"not know the stage of its caller: " + error.what());
		}
	}
}

// Emits the whole module: the capabilities, the memory model and the
// file-scope globals once, then every entry point. Returns the reflection
// document.
std::string Emitter::run() {
	_builder.setSection(spirv::Section::Capabilities);
	_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Shader) });
	if (_options.linkageObject) {
		_builder.emit(spirv::OpCapability, { static_cast<uint32_t>(spirv::Capability::Linkage) });
	}

	// The capability goes with the addressing model rather than with the block:
	// the model is declared whether or not the shader has a buffer, so a kernel
	// with no buffer parameter would otherwise name a capability it never
	// declared.
	_builder.setSection(spirv::Section::Capabilities);
	_builder.emit(spirv::OpCapability,
		{ static_cast<uint32_t>(spirv::Capability::PhysicalStorageBufferAddresses) });

	_builder.setSection(spirv::Section::MemoryModel);
	// PhysicalStorageBuffer64, because a buffer is reached by the address the
	// binding-0 block hands the shader rather than by a descriptor of its own.
	_builder.emit(spirv::OpMemoryModel,
		{ static_cast<uint32_t>(spirv::AddressingModel::PhysicalStorageBuffer64),
		  static_cast<uint32_t>(spirv::MemoryModel::GLSL450) });


	_uintType = _types.scalar(ScalarKind::UInt);
	_intType = _types.scalar(ScalarKind::Int);
	_boolType = _types.scalar(ScalarKind::Bool);
	_voidType = _types.voidType();

	// Before the parameters, since a constant's initialiser may name a struct
	// and a struct's members have to be declared before the constant that holds
	// one of them.
	declareGlobals();

	// The function type, declared once for the module. Every entry point is a
	// void function, and a duplicate OpTypeFunction is a hard validation
	// failure, so this cannot be emitted per entry point.
	_builder.setSection(spirv::Section::TypesGlobals);
	// No parameter count: like OpTypeStruct, the trailing variadic count is
	// derived from the instruction's word count and is not in the binary.
	// Writing one is read as a parameter type, which shows up as "Id is 0".
	const Id functionType = _builder.emitDecl(spirv::OpTypeFunction, { _voidType });
	_functionTypes[{ _voidType }] = functionType;

	_overloadedNames = overloadedNames(_unit);
	resolveSpecializations();
	// Where each name is first declared, so a call that finds it among several names can tell
	// whether it is visible yet.
	std::map<std::string, size_t> firstDeclared;
	for (const FunctionDecl& helper: _unit.helpers) {
		firstDeclared.emplace(helper.name, helper.order);
	}
	for (const FunctionDecl& helper: _unit.helpers) {
		if (!_overloadedNames.count(helper.name)) {
			if (helper.body) {
				HelperFunction& function = _helpers[helper.name];
				function.definition = &helper;
				function.firstOrder = firstDeclared.at(helper.name);
			}
			continue;
		}
		// An overloaded name has every signature registered, declared or defined, so a
		// call can be refused for the right one.
		const auto entry = _helpers.try_emplace(helperKey(helper, _overloadedNames));
		HelperFunction& function = entry.first->second;
		if (entry.second) {
			function.definition = &helper;
			function.firstOrder = helper.order;
			_overloads[helper.name].push_back(&function);
		} else if (helper.body && !function.definition->body) {
			function.definition = &helper;
		}
	}
	// A name the unit only declares stands for the declaration. Not a standard library
	// redeclaration (a call to it is the builtin), and not a member, whose call
	// resolves through its struct.
	// (In an executable too: a call resolved among several names must see it, and then refuse it.)
	{
		for (const FunctionDecl& helper: _unit.helpers) {
			if (helper.body || helper.name.compare(0, stdlibPrefix.size(), stdlibPrefix) == 0
				|| isMemberName(helper.name) || _overloadedNames.count(helper.name)) {
				continue;
			}
			const auto entry = _helpers.try_emplace(helper.name);
			if (!entry.first->second.definition) {
				entry.first->second.definition = &helper;
				entry.first->second.firstOrder = firstDeclared.at(helper.name);
			}
		}
	}

	if (_options.linkageObject) {
		emitImports();
	}

	// One reflection entry per entry point, in the order they were selected.
	std::string document = "{\n";
	document += "\t\"reflection_version\": 2,\n";
	document += "\t\"entry_points\": [\n";

	for (size_t k = 0; k < _entryPoints.size(); ++k) {
		_entryPoint = _entryPoints[k];

		// Per entry point rather than per module: a name bound in one
		// function is not visible in another, the buffers are that entry
		// point's own, and the interface is the list this OpEntryPoint names.
		_bindings.clear();
		bindFileScopeArrays(_entryPoint->globalsBefore);
		_bufferMembers.clear();
		_bufferBoolTypes.clear();
		_bufferMatrixTypes.clear();
		_bufferBases.clear();
		_interface.clear();
		for (const FileScopeArray& array: _fileScopeArrays) {
			_interface.push_back(array.variable);
		}
		_reflection.clear();
		_embeddedSamplers.clear();
		_fileScopeSamplerBindings.clear();
		_nextBinding = 0;
		// The address block is per entry point too, and clearing it with the
		// rest is what keeps a later change to when it is read from inheriting
		// the previous function's. Nothing reads it while _bufferMembers is
		// empty today, so this is not a fix for an observable case.
		_addressBlock = {};
		_outputs.clear();
		_stageIn = nullptr;
		_threadsPerThreadgroup = nullptr;
		_globalInvocationVariable = InvalidId;
		_masksGridSize = false;
		_stageInputs.clear();
		_attachmentParameters.clear();
		_attachmentIndices.clear();
		_hasFragCoordInput = false;
		_terminated = false;

		emitEntryPoint(functionType);

		document += "\t\t{\n";
		document += "\t\t\t\"name\": \"" + _entryPoint->name + "\",\n";
		document += std::string("\t\t\t\"stage\": \"")
			+ (_entryPoint->stage == Stage::Kernel ? "compute"
				: _entryPoint->stage == Stage::Vertex ? "vertex" : "fragment")
			+ "\",\n";
		document += "\t\t\t\"local_size\": [" + std::to_string(_options.localSizeX) + ", "
			+ std::to_string(_options.localSizeY) + ", "
			+ std::to_string(_options.localSizeZ) + "],\n";
		if (_masksGridSize) {
			document += "\t\t\t\"grid_size_spec_ids\": [" + std::to_string(kGridSizeSpecializationID) + ", "
				+ std::to_string(kGridSizeSpecializationID + 1) + ", "
				+ std::to_string(kGridSizeSpecializationID + 2) + "],\n";
		}
		const std::string functionConstants = functionConstantsJson();
		if (!functionConstants.empty()) {
			document += "\t\t\t\"function_constants\": [" + functionConstants + "\n\t\t\t],\n";
		}
		document += "\t\t\t\"bindings\": [\n";
		document += _reflection;
		document += "\t\t\t],\n";
		document += "\t\t\t\"embedded_samplers\": [";
		for (size_t e = 0; e < _embeddedSamplers.size(); ++e) {
			const SamplerState& state = _embeddedSamplers[e].state;
			document += std::string(e ? "," : "") + "\n\t\t\t\t{ \"s_address\": \""
				+ samplerAddressName(state.sAddress)
				+ "\", \"t_address\": \"" + samplerAddressName(state.tAddress)
				+ "\", \"r_address\": \"" + samplerAddressName(state.rAddress)
				+ "\", \"mag_filter\": \"" + samplerFilterName(state.magFilter)
				+ "\", \"min_filter\": \"" + samplerFilterName(state.minFilter)
				+ "\", \"mip_filter\": \"" + samplerMipFilterName(state.mipFilter)
				+ "\", \"normalized_coordinates\": " + (state.normalizedCoordinates ? "true" : "false")
				+ ", \"compare_function\": \"" + samplerCompareName(state.compare) + "\", \"anisotropy\": "
				+ std::to_string(state.anisotropy) + ", \"border_color\": \"" + samplerBorderName(state.border)
				+ "\", \"lod_min\": " + lodNumber(state.lodMin) + ", \"lod_max\": " + lodNumber(state.lodMax)
				+ " }";
		}
		document += _embeddedSamplers.empty() ? "]\n" : "\n\t\t\t]\n";
		document += k + 1 < _entryPoints.size() ? "\t\t},\n" : "\t\t}\n";
	}

	document += "\t]\n}\n";

	if (_options.linkageObject) {
		emitLibraryHelpers();
	}

	// After the library helpers, which can be the only code that declares half.
	if (_types.declaresHalf()) {
		_builder.setSection(spirv::Section::ExecutionModes);
		for (const Id entry: _entryPointIds) {
			_builder.emitNoResult(spirv::OpExecutionMode, { entry,
				static_cast<uint32_t>(spirv::ExecutionMode::SignedZeroInfNanPreserve), 16u });
		}
	}

	// After every entry point, so a diagnostic about one function's own
	// interface comes before one about how two of them pair up.
	checkStageInterfacesAgree();

	return document;
}

std::string emitModule(spirv::Builder& builder, const TranslationUnit& unit,
	const std::vector<const FunctionDecl*>& entryPoints, const ModuleOptions& options) {
	validateHelpers(unit, options.linkageObject);
	ModuleOptions chosen = options;
	if (options.linkageObject) {
		// Each helper is tried alone in a scratch module. One that needs a stage cannot be
		// exported; with an entry point it stays local to that stage's copies, and without
		// one nothing could ever call it, so it is an error.
		std::set<std::string> exportable;
		const std::vector<const FunctionDecl*> none;
		for (const FunctionDecl& helper: unit.helpers) {
			if (!helper.body || helper.isLocal) {
				continue;
			}
			ModuleOptions probe = options;
			probe.exportOnly = std::set<std::string> { helper.name };
			spirv::Builder scratch;
			TypeTable scratchTypes(scratch, unit);
			Emitter attempt(scratch, unit, none, probe, scratchTypes);
			try {
				attempt.run();
				exportable.insert(helper.name);
			} catch (const CompileError&) {
				if (entryPoints.empty()) {
					throw;
				}
			}
		}
		chosen.exportOnly = std::move(exportable);
	}
	TypeTable types(builder, unit);
	Emitter emitter(builder, unit, entryPoints, chosen, types);
	std::string result = emitter.run();
	if (emitter.sawDeadCode()) {
		// Apple type-checks code after a return, but lowering it would put its types and
		// capabilities in the module, so it is lowered once more in a module that is dropped.
		spirv::Builder scratch;
		TypeTable scratchTypes(scratch, unit);
		Emitter check(scratch, unit, entryPoints, chosen, scratchTypes);
		check.lowerDeadCode();
		check.run();
	}
	return result;
}

} // namespace mslc
