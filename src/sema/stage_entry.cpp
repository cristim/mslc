#include "sema/emitter.h"

namespace mslc {

void Emitter::declareStageOutputs() {
	const Type& type = _entryPoint->returnType;
	if (type.namedType.empty() && !type.isPointer && type.scalar == ScalarKind::Void) {
		return;
	}

	const std::string spelled = typeName(type);
	if (_entryPoint->stage == Stage::Kernel) {
		throw CompileError("a kernel returns void, and \"" + _entryPoint->name + "\" returns "
			+ spelled);
	}

	if (type.namedType.empty()) {
		if (_entryPoint->stage == Stage::Vertex) {
			// A bare float4 return is the vertex position (MSL 5.2.3).
			const std::string what = "the value vertex function \"" + _entryPoint->name + "\" returns";
			if (type.isPointer || type.isMatrix() || type.vectorWidth != 4 || type.scalar != ScalarKind::Float) {
				throw CompileError("vertex function \"" + _entryPoint->name + "\" returns " + spelled
					+ ", and a vertex function returns a struct with a [[position]] field or a float4 "
					"position");
			}
			_outputs.push_back(declareStageVariable(type, spirv::StorageClass::Output, what));
			_builder.emit(spirv::OpDecorate, { _outputs.back().variable,
				static_cast<uint32_t>(spirv::Decoration::BuiltIn),
				static_cast<uint32_t>(spirv::BuiltIn::Position) });
			return;
		}

		_outputs.push_back(declareStageVariable(type, spirv::StorageClass::Output,
			"the value fragment function \"" + _entryPoint->name + "\" returns"));
		_builder.emit(spirv::OpDecorate, { _outputs.back().variable,
			static_cast<uint32_t>(spirv::Decoration::Location), 0u });
		return;
	}

	const StructDecl* decl = structValue(type);
	if (_entryPoint->stage == Stage::Fragment) {
		if (!decl) {
			throw CompileError("fragment function \"" + _entryPoint->name + "\" returns " + spelled
				+ ", and mslc lowers a returned struct this source declares and nothing else");
		}
		_outputs = declareFragmentOutputs(*decl);
		return;
	}

	if (!decl) {
		throw CompileError("vertex function \"" + _entryPoint->name + "\" returns " + spelled
			+ ", and mslc lowers a returned struct this source declares and nothing else");
	}

	_outputs = declareStageStruct(*decl, spirv::StorageClass::Output);
}

// Framebuffer fetch, [[color(n)]] on a fragment input field or parameter: Vulkan reads the
// colour attachment a previous subpass wrote only through an input attachment, so the value
// is an OpImageRead of one whose InputAttachmentIndex is n. The descriptor is bound by
// declareAttachments, after every texture and sampler binding of the entry point.
StageVariable Emitter::declareFramebufferFetch(const Type& type, uint32_t index, const std::string& name,
	const std::string& what) {
	constexpr uint32_t maxColorAttachments = 8;
	if (_entryPoint->stage != Stage::Fragment) {
		throw CompileError(what + " is [[color(n)]], which only a fragment function reads");
	}
	const bool sampled = type.scalar == ScalarKind::Float || type.scalar == ScalarKind::Half
		|| type.scalar == ScalarKind::Int || type.scalar == ScalarKind::UInt;
	if (type.isPointer || type.arrayLength || !type.namedType.empty() || type.isMatrix()
		|| type.isPacked || type.vectorWidth != 4 || !sampled) {
		throw CompileError(what + " is [[color(n)]] with type " + typeName(type) + ", and mslc "
			"reads a colour attachment only as a float4, half4, int4 or uint4");
	}
	if (index >= maxColorAttachments) {
		throw CompileError(what + " is [[color(" + std::to_string(index) + ")]], and Apple's "
			"compiler allows indices 0 to 7");
	}
	if (!_attachmentIndices.insert(index).second) {
		throw CompileError(what + " reuses [[color(" + std::to_string(index) + ")]], which "
			"another input of this function already reads");
	}

	// Apple pairs the fetched colour with the output of the same index and refuses a pair of
	// different types, so a half4 read cannot be written back as a float4.
	const Type& returned = _entryPoint->returnType;
	const Type* output = nullptr;
	if (const StructDecl* decl = structValue(returned)) {
		for (const StructField& field: decl->fields) {
			if (field.attributes.colorIndex == index) {
				output = &field.type;
			}
		}
	} else if (index == 0 && returned.scalar != ScalarKind::Void && !returned.isPointer) {
		output = &returned;
	}
	if (output && (output->scalar != type.scalar || output->vectorWidth != type.vectorWidth)) {
		throw CompileError(what + ": 'color' attachment " + std::to_string(index) + " has different "
			"types on input (" + typeName(type) + ") and output (" + typeName(*output) + ")");
	}

	Type carried = type;
	if (carried.scalar == ScalarKind::Half) {
		carried.scalar = ScalarKind::Float;
	}
	StageVariable fetch;
	fetch.attachment = index;
	fetch.sampled = carried.scalar;
	fetch.name = name;
	fetch.valueType = declaredTypeOf(type);
	fetch.interfaceType = declaredTypeOf(carried);
	return fetch;
}

// One input attachment descriptor per fetched colour, after the textures and samplers and
// in colour order, in the fragment function's set. A consumer binds the attachment's
// view there and maps pInputAttachments[input_attachment_index] to the colour attachment.
void Emitter::declareAttachments() {
	std::vector<StageVariable*> fetches;
	for (StageVariable& input: _stageInputs) {
		if (input.attachment) {
			fetches.push_back(&input);
		}
	}
	for (AttachmentParameter& fetched: _attachmentParameters) {
		fetches.push_back(&fetched.variable);
	}
	std::sort(fetches.begin(), fetches.end(), [](const StageVariable* a, const StageVariable* b) {
		return *a->attachment < *b->attachment;
	});

	for (StageVariable* fetch: fetches) {
		const uint32_t binding = _nextBinding++;
		const Id imageType = _types.subpassImage(fetch->sampled);
		fetch->variable = declareDescriptorVariable(imageType, binding);
		_builder.emit(spirv::OpDecorate, { fetch->variable,
			static_cast<uint32_t>(spirv::Decoration::InputAttachmentIndex), *fetch->attachment });
		addReflectionEntry("{ \"kind\": \"InputAttachment\", \"metal_index\": "
			+ std::to_string(*fetch->attachment) + ", \"descriptor\": " + descriptorJson(binding)
			+ ", \"input_attachment_index\": " + std::to_string(*fetch->attachment)
			+ ", \"scalar\": \"" + std::string(fetch->sampled == ScalarKind::Float ? "float"
				: fetch->sampled == ScalarKind::Int ? "int" : "uint") + "\""
			+ ", \"name\": \"" + fetch->name + "\" }");
	}
}

Id Emitter::readAttachment(const StageVariable& attachment) {
	const Id imageType = _types.subpassImage(attachment.sampled);
	const Id image = loadFrom(attachment.variable, imageType);
	const Id origin = _types.zero(_types.vector(ScalarKind::Int, 2));
	const Id texel = _builder.emitTyped(spirv::OpImageRead, attachment.interfaceType, { image, origin });
	return convert(texel, attachment.interfaceType, attachment.valueType);
}

// The [[stage_in]] parameter becomes a local holding the struct, filled from
// the Input variables at the top of the function, so a field of it is read
// the way a field of any local is.
void Emitter::loadStageInputs() {
	for (const AttachmentParameter& fetched: _attachmentParameters) {
		const Id declared = fetched.variable.valueType;
		const Id local = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Function, declared),
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });
		_builder.emit(spirv::OpStore, { local, readAttachment(fetched.variable) });
		bindLocal(fetched.parameter->name, local, declared, fetched.parameter->type);
	}

	if (!_stageIn) {
		return;
	}

	const Id structType = declaredTypeOf(_stageIn->type);
	std::vector<uint32_t> fields;
	for (const StageVariable& input: _stageInputs) {
		if (input.attachment) {
			fields.push_back(readAttachment(input));
			continue;
		}
		const Id loaded = loadFrom(input.variable, input.interfaceType);
		fields.push_back(convert(loaded, input.interfaceType, input.valueType));
	}

	const Id local = _builder.emitDeclTyped(spirv::OpVariable,
		_types.pointer(spirv::StorageClass::Function, structType),
		{ static_cast<uint32_t>(spirv::StorageClass::Function) });
	_builder.emit(spirv::OpStore, { local,
		_builder.emitTyped(spirv::OpCompositeConstruct, structType, fields) });

	bindLocal(_stageIn->name, local, structType, _stageIn->type);
}

// [[threads_per_threadgroup]] is the workgroup size, which the module holds as
// three spec constants for indium to fill in. indium refuses a partial
// threadgroup, so the size is the same for every group. The parameter lives
// in the function's own storage, as a by-value parameter does.
void Emitter::loadThreadsPerThreadgroup() {
	if (!_threadsPerThreadgroup) {
		return;
	}

	const Type& type = _threadsPerThreadgroup->type;
	const Id declared = declaredTypeOf(type);
	const Id component = _types.scalar(type.scalar);
	const uint32_t width = type.isScalar() ? 1 : type.vectorWidth;
	std::vector<uint32_t> parts;
	for (uint32_t i = 0; i < width; ++i) {
		parts.push_back(type.scalar == ScalarKind::UInt ? _localSize[i]
			: _builder.emitTyped(spirv::OpUConvert, component, { _localSize[i] }));
	}

	const Id value = width == 1 ? parts[0]
		: _builder.emitTyped(spirv::OpCompositeConstruct, declared, parts);
	const Id local = _builder.emitDeclTyped(spirv::OpVariable,
		_types.pointer(spirv::StorageClass::Function, declared),
		{ static_cast<uint32_t>(spirv::StorageClass::Function) });
	_builder.emit(spirv::OpStore, { local, value });

	bindLocal(_threadsPerThreadgroup->name, local, declared, type);
}

// Every entry point is a void function, so a returned value is written to
// the Output variables and the function returns nothing.
void Emitter::emitReturn(const Statement& statement) {
	if (_helper) {
		emitHelperReturn(statement);
		return;
	}

	if (!statement.expression) {
		if (!_outputs.empty()) {
			throw CompileError("\"" + _entryPoint->name + "\" returns "
				+ typeName(_entryPoint->returnType) + ", so a return needs a value");
		}

		terminate(spirv::OpReturn, { });
		return;
	}

	if (_outputs.empty()) {
		emitVoidReturnValue(*statement.expression, "\"" + _entryPoint->name + "\"");
		terminate(spirv::OpReturn, { });
		return;
	}

	requireEnumValue(_entryPoint->returnType.enumName, *statement.expression,
		"the return value of \"" + _entryPoint->name + "\"");
	const Id value = emitExpression(*statement.expression);
	const Id declared = declaredTypeOf(_entryPoint->returnType);

	if (!_entryPoint->returnType.namedType.empty()) {
		// The fields are extracted one by one, so a buffer's form of the struct
		// serves as it is unless a packed member has to change shape.
		Id copy = value;
		TypeTable::StructForm have;
		TypeTable::StructForm want;
		const bool sameMembers = _types.structForm(_builder.typeOf(value), have)
			&& _types.structForm(declared, want)
			&& have.name == want.name && have.declared == want.declared;
		if (!sameMembers) {
			copy = convertImplicit(value, declared);
		}

		for (size_t i = 0; i < _outputs.size(); ++i) {
			const StageVariable& output = _outputs[i];
			const Id field = _builder.emitTyped(spirv::OpCompositeExtract, output.valueType,
				{ copy, static_cast<uint32_t>(i) });
			const Id stored = structValue(_entryPoint->returnType)->fields[i].attributes.sampleMask
				? _builder.emitTyped(spirv::OpCompositeConstruct, output.interfaceType, { field })
				: convert(field, output.valueType, output.interfaceType);
			_builder.emit(spirv::OpStore, { output.variable, stored });
		}
	} else {
		const StageVariable& output = _outputs.front();
		const Id returned = convertImplicit(value, declared);
		_builder.emit(spirv::OpStore, { output.variable,
			convert(returned, output.valueType, output.interfaceType) });
	}

	terminate(spirv::OpReturn, { });
}

// Locations follow stageLocations, where Apple pairs a vertex output with a
// fragment input by name (the [[user(name)]] name where a field has one) and
// type. A vertex and a fragment function pair when every field the fragment
// reads is one the vertex function writes, and such a pair agrees only when
// each of those fields is at the same Location in both. A fragment in another
// library cannot be checked.
void Emitter::checkStageInterfacesAgree() const {
	struct InterfaceField {
		std::string name;
		std::string type;
		uint32_t location;
		bool operator==(const InterfaceField& other) const {
			return name == other.name && type == other.type;
		}
	};

	const auto interfaceFields = [this](const std::string& name) {
		std::vector<InterfaceField> fields;
		if (const StructDecl* decl = _unit.findStruct(name)) {
			const auto locations = stageLocations(*decl);
			for (size_t index = 0; index < decl->fields.size(); ++index) {
				const StructField& field = decl->fields[index];
				if (!field.attributes.position && !field.attributes.pointSize && !field.attributes.colorIndex) {
					fields.push_back({ field.attributes.userName.value_or(field.name),
						typeName(field.type), *locations[index] });
				}
			}
		}
		return fields;
	};

	for (const FunctionDecl* vertex: _entryPoints) {
		if (vertex->stage != Stage::Vertex || vertex->returnType.namedType.empty()) {
			continue;
		}

		for (const FunctionDecl* fragment: _entryPoints) {
			if (fragment->stage != Stage::Fragment) {
				continue;
			}

			const auto outputs = interfaceFields(vertex->returnType.namedType);
			for (const Parameter& parameter: fragment->parameters) {
				if (!parameter.attributes.stageIn) {
					continue;
				}

				// Apple pairs by name and type, so a fragment reading a field this
				// vertex function does not write is not its pipeline partner.
				const auto inputs = interfaceFields(parameter.type.namedType);
				const bool pairs = std::all_of(inputs.begin(), inputs.end(), [&](const auto& input) {
					return std::find(outputs.begin(), outputs.end(), input) != outputs.end();
				});
				const bool agrees = std::all_of(inputs.begin(), inputs.end(), [&](const auto& input) {
					return std::find_if(outputs.begin(), outputs.end(), [&](const auto& output) {
						return output == input && output.location == input.location;
					}) != outputs.end();
				});
				if (pairs && !agrees) {
					throw CompileError("vertex function \"" + vertex->name + "\" returns "
						+ vertex->returnType.namedType + " and fragment function \"" + fragment->name
						+ "\" takes " + typeName(parameter.type) + " as [[stage_in]]; mslc gives "
						"their fields Locations by declaration order, so the fragment's fields, other "
						"than [[position]], have to be the vertex function's first ones, in order "
						"(or be named [[user(locnN)]], which is Location N in both)");
				}
			}
		}
	}

	// No vertex pairs by name+type; if one covers every input name, report the
	// first type mismatch rather than staying silent.
	for (const FunctionDecl* fragment: _entryPoints) {
		if (fragment->stage != Stage::Fragment) {
			continue;
		}

		for (const Parameter& parameter: fragment->parameters) {
			if (!parameter.attributes.stageIn) {
				continue;
			}

			const auto inputs = interfaceFields(parameter.type.namedType);

			bool fullyPaired = false;
			for (const FunctionDecl* vertex: _entryPoints) {
				if (vertex->stage != Stage::Vertex || vertex->returnType.namedType.empty()) {
					continue;
				}
				const auto outputs = interfaceFields(vertex->returnType.namedType);
				const bool pairs = std::all_of(inputs.begin(), inputs.end(), [&](const auto& input) {
					return std::find(outputs.begin(), outputs.end(), input) != outputs.end();
				});
				if (pairs) {
					fullyPaired = true;
					break;
				}
			}
			if (fullyPaired) {
				continue;
			}

			for (const FunctionDecl* vertex: _entryPoints) {
				if (vertex->stage != Stage::Vertex || vertex->returnType.namedType.empty()) {
					continue;
				}
				const auto outputs = interfaceFields(vertex->returnType.namedType);
				const bool namesCovered = std::all_of(inputs.begin(), inputs.end(), [&](const auto& input) {
					return std::find_if(outputs.begin(), outputs.end(), [&](const auto& output) {
						return output.name == input.name;
					}) != outputs.end();
				});
				if (!namesCovered) {
					continue;
				}

				for (const auto& input: inputs) {
					const auto candidate = std::find_if(outputs.begin(), outputs.end(), [&](const auto& output) {
						return output.name == input.name;
					});
					// namesCovered already guarantees candidate != outputs.end().
					if (candidate->type != input.type) {
						throw CompileError("vertex function \"" + vertex->name + "\" writes \""
							+ input.name + "\" as " + candidate->type + " and fragment function \""
							+ fragment->name + "\" reads it as " + input.type + "; mslc requires a "
							"stage-crossing field to have one type on both sides, since Apple rejects "
							"the mismatch when the pipeline is created");
					}
				}
				break;
			}
		}
	}
}


// A local variable becomes a Function-storage pointer, which the
// expression path then loads from, so a local and a parameter behave the
// same way when a name is used.
// T& r = place; names the place the initialiser denotes, so a use of r loads
// from it and a store through r writes to it. A local variable is aliased by
// copying its binding; an element or a member, or a reference parameter, is a
// place reached by address, which the binding then holds.
void Emitter::emitReferenceDeclaration(const VariableDeclaration& declaration) {
	const Expression& place = *declaration.initializer;
	if (declaration.type.isAuto) {
		if (const auto given = enumOf(place); given && !given->empty()) {
			throw CompileError("the auto variable \"" + declaration.name + "\" would have the enum type "
				+ *given + ", which is not lowered yet; write the enum type");
		}
	} else if ((declaration.type.enumName == "metal::component") != isComponentValue(place)) {
		throw CompileError("the reference \"" + declaration.name + "\" is not the type of the place "
			"it is bound to");
	}
	if (place.kind != ExpressionKind::Identifier && place.kind != ExpressionKind::Index
		&& place.kind != ExpressionKind::Member) {
		throw CompileError("the reference \"" + declaration.name + "\" has to be bound to a variable, "
			"an element or a member; binding it to a value is not lowered yet");
	}

	const bool vectorComponent = isVectorComponent(place);
	if (vectorComponent || (bindsTemporary(place) && declaration.type.isConst
		&& declaration.type.addressSpace == AddressSpace::Thread)) {
		// A reference to a component of a vector has no storage to name. Apple refuses a non-const
		// one, and binds a const one to a temporary copy of the component.
		if (vectorComponent && !declaration.type.isConst) {
			throw CompileError("the reference \"" + declaration.name + "\" is bound to a component of a "
				"vector, which a non-const reference cannot name (Apple refuses it too); copy the "
				"component or bind a const reference");
		}
		if (vectorComponent && declaration.type.addressSpace != AddressSpace::Thread) {
			throw CompileError("the reference \"" + declaration.name + "\" to a component of a vector has to be "
				"spelled thread: Apple refuses a reference with no address space, and one in another space "
				"cannot bind the thread copy");
		}
		if (!vectorComponent) rejectPlaceThroughConditional(place, "binding a reference to");
		const Id value = emitExpression(place);
		const Type referenced = declaration.type.isAuto
			? deduceAutoType(declaration.type, _builder.typeOf(value), declaration.name) : declaration.type;
		requireSameReferent(declaration, referenced, _builder.typeOf(value));
		const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Function, _builder.typeOf(value)),
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });
		_builder.emit(spirv::OpStore, { variable, value });
		bindLocal(declaration.name, variable, _builder.typeOf(value), referenced);
		return;
	}
	if (bindsTemporary(place) && !declaration.type.isConst
		&& declaration.type.addressSpace != AddressSpace::Constant) {
		throw CompileError("the reference \"" + declaration.name + "\" is bound to a member or element of "
			"a temporary, which a non-const reference cannot bind (Apple refuses it too)");
	}
	rejectPlaceThroughConditional(place, "binding a reference to");

	const Expression& root = storeRoot(place);
	if (root.kind != ExpressionKind::Identifier || isComputedValue(place)) {
		throw CompileError("the reference \"" + declaration.name + "\" is bound to a place mslc "
			"cannot name");
	}
	const Binding* rootBinding = findBinding(root);
	const bool rootReadOnly = (rootBinding && rootBinding->readOnly)
		|| (!rootBinding && _constants.count(root.name));
	const bool constReference = declaration.type.isConst
		|| declaration.type.addressSpace == AddressSpace::Constant;
	if (rootReadOnly && !constReference) {
		throw CompileError("the reference \"" + declaration.name + "\" is not const but is bound to "
			"constant memory or a const variable");
	}

	if (place.kind == ExpressionKind::Identifier && rootBinding
		&& rootBinding->bufferPointeeType == InvalidId && rootBinding->isPointer
		&& rootBinding->storageClass == spirv::StorageClass::Function) {
		Binding alias = *rootBinding;
		alias.readOnly = alias.readOnly || constReference;
		const Type referenced = declaration.type.isAuto
			? deduceAutoType(declaration.type, alias.pointeeType, declaration.name) : declaration.type;
		requireSameReferent(declaration, referenced, alias.pointeeType);
		_bindings[declaration.name] = alias;
		return;
	}

	const Id address = emitPlaceAddress(place);
	const Id pointee = valueTypeAt(address);
	const auto storageClass = _types.storageClassOf(_builder.typeOf(address));
	if (pointee == InvalidId || !storageClass) {
		throw CompileError("cannot determine what the reference \"" + declaration.name + "\" names");
	}
	const Type referenced = declaration.type.isAuto
		? deduceAutoType(declaration.type, pointee, declaration.name) : declaration.type;
	requireSameReferent(declaration, referenced, pointee);

	Binding binding;
	binding.id = address;
	binding.isPointer = true;
	binding.pointeeType = pointee;
	binding.storageClass = *storageClass;
	binding.pointeeMsl = referenced;
	binding.structType = referenced.namedType.empty()
		? nullptr : _unit.findStruct(referenced.namedType);
	binding.readOnly = constReference || rootReadOnly;
	binding.isAddress = true;
	_bindings[declaration.name] = binding;
}

// A reference binds without a conversion: a different type would be a
// temporary, which is a value and not the place.
void Emitter::requireSameReferent(const VariableDeclaration& declaration, const Type& type, Id referent) {
	if (!type.namedType.empty()) {
		TypeTable::StructForm have;
		if (!_types.structForm(referent, have) || have.name != type.namedType) {
			throw CompileError("the reference \"" + declaration.name + "\" is not the type of the place "
				"it is bound to");
		}
		return;
	}
	if (referent != declaredTypeOf(type)) {
		throw CompileError("the reference \"" + declaration.name + "\" is not the type of the place "
			"it is bound to; a conversion would make a temporary");
	}
}

} // namespace mslc
