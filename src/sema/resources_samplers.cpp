#include "sema/emitter.h"

namespace mslc {

// One entry per binding goes in the reflection. The separator goes before the
// entry rather than after it, because a trailing comma makes the array a
// syntax error: json.load refuses "Illegal trailing comma before end of
// array", so a reflection a reader cannot parse is not a reflection.
void Emitter::addReflectionEntry(const std::string& entry) {
	if (!_reflection.empty()) {
		_reflection.pop_back();  // the newline the previous entry ended with
		_reflection += ",\n";
	}

	_reflection += "\t\t\t\t" + entry + "\n";
}

// A lod_clamp bound as a JSON number: the shortest decimal form of the float.
std::string lodNumber(float value) {
	char text[32];
	std::snprintf(text, sizeof text, "%.9g", static_cast<double>(value));
	return text;
}

std::string Emitter::descriptorJson(uint32_t binding) const {
	return "{ \"set\": " + std::to_string(descriptorSet()) + ", \"binding\": "
		+ std::to_string(binding) + " }";
}

// A builtin parameter is a local of the entry point that starts as the builtin's value, so the body may store to
// it ([dcl.fct]: a parameter is an ordinary variable). The Input variable cannot be written, so one the body
// writes or binds a reference to is copied into a Function variable, converted as a read converts it.
void Emitter::copyWrittenBuiltins() {
	ReferenceSlots references;
	for (const FunctionDecl& function: _unit.helpers) {
		// A member function's receiver is not among the call's arguments.
		const size_t first = !function.parameters.empty() && function.parameters[0].name == "this" ? 1 : 0;
		for (size_t i = first; i < function.parameters.size(); ++i) {
			if (function.parameters[i].isMutableReference()) {
				references.insert({ unqualifiedName(function.name), i - first });
			}
		}
	}
	std::set<std::string> written;
	collectWritten(*_entryPoint->body, references, written);
	for (const Parameter& parameter: _entryPoint->parameters) {
		const auto binding = _bindings.find(parameter.name);
		if (!parameter.attributes.builtin || !written.count(parameter.name) || binding == _bindings.end()
			|| binding->second.storageClass != spirv::StorageClass::Input) {
			continue;
		}
		Expression read;
		read.kind = ExpressionKind::Identifier;
		read.name = parameter.name;
		const Id type = declaredTypeOf(parameter.type);
		const Id loaded = emitExpression(read);
		const Id value = convert(loaded, _builder.typeOf(loaded), type);
		const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Function, type), { static_cast<uint32_t>(spirv::StorageClass::Function) });
		_builder.emit(spirv::OpStore, { variable, value });
		bindLocal(parameter.name, variable, type, parameter.type);
	}
}

// A UniformConstant variable of the pointee's type at the next descriptor
// binding of this entry point's set.
Id Emitter::declareDescriptorVariable(Id pointee, uint32_t binding) {
	const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
		_types.pointer(spirv::StorageClass::UniformConstant, pointee),
		{ static_cast<uint32_t>(spirv::StorageClass::UniformConstant) });
	_builder.emit(spirv::OpDecorate, { variable,
		static_cast<uint32_t>(spirv::Decoration::DescriptorSet), descriptorSet() });
	_builder.emit(spirv::OpDecorate, { variable,
		static_cast<uint32_t>(spirv::Decoration::Binding), binding });
	_interface.push_back(variable);
	return variable;
}

// Textures take the bindings after the address block, in declaration order,
// then the samplers, then the constexpr samplers as the body declares them.
// That is Iridium's order (indium src/iridium/air.cpp: the texture loop, the
// sampler loop, then air.sampler_states), and it is what indium binds by.
// The index among its own kind is the Metal index, and an unattributed
// parameter takes the smallest one no attribute claims, as Apple's does.
void Emitter::declareResources(const std::vector<std::pair<size_t, const Parameter*>>& resources) {
	_nextBinding = _bufferMembers.empty() ? 0u : 1u;

	struct Entry {
		size_t index;
		const Parameter* parameter;
		uint32_t metalIndex;
	};
	std::vector<Entry> textures;
	std::vector<Entry> samplers;
	// An unattributed parameter takes the smallest index of its kind that no
	// attribute anywhere in the list claims and no earlier parameter took.
	std::set<uint32_t> taken[2];
	for (const auto& [index, parameter]: resources) {
		(void)index;
		const bool isTexture = parameter->type.isTexture();
		const auto& declared = isTexture ? parameter->attributes.textureIndex
			: parameter->attributes.samplerIndex;
		if (declared) {
			taken[isTexture ? 0 : 1].insert(*declared);
		}
	}

	for (const auto& [index, parameter]: resources) {
		const bool isTexture = parameter->type.isTexture();
		const auto& declared = isTexture ? parameter->attributes.textureIndex
			: parameter->attributes.samplerIndex;
		uint32_t metalIndex = 0;
		if (declared) {
			metalIndex = *declared;
		} else {
			std::set<uint32_t>& claimed = taken[isTexture ? 0 : 1];
			while (claimed.count(metalIndex)) {
				++metalIndex;
			}
			const uint32_t limit = isTexture ? 127 : 15;
			if (metalIndex > limit) {
				throw CompileError(std::string("no '") + (isTexture ? "texture" : "sampler")
					+ "' resource location is available for parameter \"" + parameter->name
					+ "\": every index up to " + std::to_string(limit) + " is taken");
			}
			claimed.insert(metalIndex);
		}
		(isTexture ? textures : samplers).push_back({ index, parameter, metalIndex });
	}

	for (const Entry& entry: textures) {
		const uint32_t binding = _nextBinding++;
		const Id imageType = resourcePointee(entry.parameter->type);
		const Id variable = declareDescriptorVariable(imageType, binding);
		const bool texelBuffer = entry.parameter->type.resource == ResourceKind::TextureBuffer;
		if (entry.parameter->type.textureAccess == TextureAccess::Write) {
			_builder.emit(spirv::OpDecorate, { variable, static_cast<uint32_t>(spirv::Decoration::NonReadable) });
		}

		Binding bound;
		bound.id = variable;
		bound.isPointer = true;
		bound.pointeeType = imageType;
		bound.storageClass = spirv::StorageClass::UniformConstant;
		bound.pointeeMsl = entry.parameter->type;
		_bindings[entry.parameter->name] = bound;

		// A texel buffer is its own kind, so that a consumer that binds every "Texture" as an
		// image view skips it rather than binding the wrong descriptor type.
		addReflectionEntry(std::string("{ \"kind\": \"") + (texelBuffer ? "TextureBuffer" : "Texture")
			+ "\", \"metal_index\": "
			+ std::to_string(entry.metalIndex)
			+ ", \"descriptor\": " + descriptorJson(binding)
			+ (texelBuffer ? std::string(", \"descriptor_type\": \"")
				+ (entry.parameter->type.isStorageTexture() ? "StorageTexelBuffer" : "UniformTexelBuffer") + "\"" : std::string())
			+ ", \"texture_access\": \""
			+ (entry.parameter->type.textureAccess == TextureAccess::Write ? "Write"
				: entry.parameter->type.textureAccess == TextureAccess::Read ? "Read"
				: entry.parameter->type.textureAccess == TextureAccess::ReadWrite ? "ReadWrite" : "Sample") + "\""
			+ ", \"param_index\": " + std::to_string(entry.index)
			+ ", \"name\": \"" + entry.parameter->name + "\" }");
	}

	for (const Entry& entry: samplers) {
		const uint32_t binding = _nextBinding++;
		const Id variable = declareDescriptorVariable(_types.samplerType(), binding);

		Binding bound;
		bound.id = variable;
		bound.isPointer = true;
		bound.pointeeType = _types.samplerType();
		bound.storageClass = spirv::StorageClass::UniformConstant;
		bound.pointeeMsl = entry.parameter->type;
		_bindings[entry.parameter->name] = bound;

		addReflectionEntry("{ \"kind\": \"Sampler\", \"metal_index\": "
			+ std::to_string(entry.metalIndex)
			+ ", \"descriptor\": " + descriptorJson(binding)
			+ ", \"param_index\": " + std::to_string(entry.index)
			+ ", \"name\": \"" + entry.parameter->name + "\" }");
	}
}

// A sampler declared in the shader is a sampler variable at the next binding,
// and the reflection carries its state, because there is nothing for the app
// to bind: indium creates the immutable sampler from it, as it does from
// Iridium's EmbeddedSampler. Equal states share one variable and one binding.
//
// Every one in the body is reserved before the entry point is emitted, since
// the variable has to be in the OpEntryPoint interface, which comes first.
bool Emitter::reserveLocalSamplers(const Statement& statement, const std::set<std::string>& used, bool reserve) {
	bool any = false;
	if (statement.declaration && statement.declaration->sampler
		&& used.count(statement.declaration->name)) {
		if (reserve) {
			reserveSampler(*statement.declaration);
		}
		any = true;
	}

	for (const StatementPtr& child: statement.children) {
		any = reserveLocalSamplers(*child, used, reserve) || any;
	}
	for (const Statement* nested: { statement.thenBranch.get(), statement.elseBranch.get(),
		statement.forBody.get(), statement.whileBody.get() }) {
		if (nested) {
			any = reserveLocalSamplers(*nested, used, reserve) || any;
		}
	}
	for (const SwitchCase& kase: statement.switchCases) {
		for (const StatementPtr& child: kase.body) {
			any = reserveLocalSamplers(*child, used, reserve) || any;
		}
	}
	return any;
}

void Emitter::reserveThreadgroupLocals(const Statement& statement) {
	if (statement.declaration) { reserveThreadgroupLocal(*statement.declaration); }
	if (statement.forInitializer) { reserveThreadgroupLocal(*statement.forInitializer); }
	for (const StatementPtr& child: statement.children) {
		reserveThreadgroupLocals(*child);
	}
	for (const StatementPtr& child: statement.switchPreamble) {
		reserveThreadgroupLocals(*child);
	}
	for (const Statement* nested: { statement.thenBranch.get(), statement.elseBranch.get(),
		statement.forBody.get(), statement.whileBody.get() }) {
		if (nested) { reserveThreadgroupLocals(*nested); }
	}
	for (const SwitchCase& kase: statement.switchCases) {
		for (const StatementPtr& child: kase.body) {
			reserveThreadgroupLocals(*child);
		}
	}
}

// Only a plain value type is reserved; every other form is refused where it is declared.
void Emitter::reserveThreadgroupLocal(const VariableDeclaration& declaration) {
	const Type& type = declaration.type;
	// An array is reserved here too. emitLocalArray looks the variable up in
	// this table when it runs, so the same shape serves both, and a
	// threadgroup array has to be in the OpEntryPoint interface just as a
	// threadgroup scalar does.
	if (type.addressSpace != AddressSpace::Threadgroup || type.isAuto || type.isPointer
		|| type.resource != ResourceKind::None || declaration.isReference
		|| declaration.initializer) {
		return;
	}
	const Id typeId = type.arrayLength ? arrayTypeOf(type) : declaredTypeOf(type);
	_threadgroupLocals[&declaration] = _builder.emitDeclTyped(spirv::OpVariable,
		_types.pointer(spirv::StorageClass::Workgroup, typeId),
		{ static_cast<uint32_t>(spirv::StorageClass::Workgroup) });
	_interface.push_back(_threadgroupLocals[&declaration]);
}

void Emitter::reserveSampler(const VariableDeclaration& declaration) {
	const bool known = std::any_of(_embeddedSamplers.begin(), _embeddedSamplers.end(),
		[&](const EmbeddedSampler& other) { return other.state == *declaration.sampler; });
	if (known) {
		return;
	}

	const uint32_t binding = _nextBinding++;
	_embeddedSamplers.push_back({ *declaration.sampler,
		declareDescriptorVariable(_types.samplerType(), binding) });

	addReflectionEntry("{ \"kind\": \"Sampler\", \"descriptor\": " + descriptorJson(binding)
		+ ", \"embedded_sampler\": " + std::to_string(_embeddedSamplers.size() - 1)
		+ ", \"name\": \"" + declaration.name + "\" }");
}

// What a call can reach, for the passes that run before emission. A member is chosen by its
// receiver's struct, which is known only when the call is emitted, so every member of that name
// is taken.
std::vector<const FunctionDecl*> Emitter::functionsCalledBy(const Expression& call) {
	std::vector<const FunctionDecl*> found;
	const auto add = [&](const HelperFunction* helper) {
		if (helper->definition->body) { found.push_back(helper->definition); }
	};
	if (call.left->kind == ExpressionKind::Identifier) {
		for (const HelperFunction* helper: helpersCalled(call)) { add(helper); }
	} else if (call.left->kind == ExpressionKind::Member) {
		const std::string suffix = "::" + call.left->memberName;
		for (const auto& entry: _helpers) {
			const FunctionDecl& definition = *entry.second.definition;
			if (definition.name.size() > suffix.size()
				&& definition.name.compare(definition.name.size() - suffix.size(), suffix.size(), suffix) == 0) {
				add(&entry.second);
			}
		}
	}
	return found;
}

// A file-scope sampler is reserved, once the entry point's own are, if the body
// names it where no parameter or local of that name hides it. They go in the
// order the file declares them. Which embedded samplers an entry point lists is
// per entry point, so a sampler two of them name is listed by each.
void Emitter::reserveFileScopeSamplers() {
	std::set<const VariableDeclaration*> named;
	const auto scan = [&](const FunctionDecl& function) {
		ScopeStack scopes(1);
		for (const Parameter& parameter: function.parameters) {
			scopes.back().insert(parameter.name);
		}
		std::set<std::string> names;
		collectUnshadowed(function.body.get(), scopes, names);

		const auto declaredAbove = _unit.globals.begin() + function.globalsBefore;
		bool any = false;
		for (auto global = _unit.globals.begin(); global != declaredAbove; ++global) {
			if (global->sampler && names.count(global->name)) {
				named.insert(&*global);
				any = true;
			}
		}
		return any;
	};

	scan(*_entryPoint);

	// Every function the entry point reaches, with what it calls; one that names a sampler,
	// or calls one that does, is emitted per entry point (helperScope).
	std::map<const FunctionDecl*, std::vector<const FunctionDecl*>> callees;
	std::set<const FunctionDecl*> naming;
	// The helpers that declare a named constexpr sampler, in the order they are reached; theirs are reserved after
	// the file-scope ones, which keep the bindings they had before helpers' own were lowered.
	std::vector<const FunctionDecl*> declaring;
	std::vector<const FunctionDecl*> pending { _entryPoint };
	callees[_entryPoint];
	std::vector<const FunctionDecl*> subscriptHelpers;
	for (const auto& [name, entry]: _helpers) {
		if (entry.definition->body && name.size() > 12 && name.compare(name.size() - 12, 12, "::operator[]") == 0)
			subscriptHelpers.push_back(entry.definition);
	}
	const auto hasIdx = [](const auto& self, const Expression* e) -> bool {
		return e && ((e->kind == ExpressionKind::Index && !e->isDereference) || self(self, e->left.get()) || self(self, e->right.get())
			|| std::any_of(e->arguments.begin(), e->arguments.end(), [&](const auto& a) { return self(self, a.get()); })
			|| std::any_of(e->elements.begin(), e->elements.end(), [&](const auto& el) { return self(self, el.value.get()); }));
	};
	const auto hasIdxStmt = [&](const auto& self, const Statement* s) -> bool {
		if (!s) return false;
		if (hasIdx(hasIdx, s->expression.get()) || hasIdx(hasIdx, s->forCondition.get()) || hasIdx(hasIdx, s->forIncrement.get())
			|| hasIdx(hasIdx, s->whileCondition.get()) || (s->declaration && hasIdx(hasIdx, s->declaration->initializer.get()))
			|| (s->forInitializer && hasIdx(hasIdx, s->forInitializer->initializer.get())) || self(self, s->thenBranch.get())
			|| self(self, s->elseBranch.get()) || self(self, s->forBody.get()) || self(self, s->whileBody.get())) return true;
		for (const auto* list: { &s->children, &s->switchPreamble })
			for (const auto& c: *list) if (self(self, c.get())) return true;
		for (const SwitchCase& k: s->switchCases) {
			if (hasIdx(hasIdx, k.value.get())) return true;
			for (const auto& c: k.body) if (self(self, c.get())) return true;
		}
		return false;
	};
	while (!pending.empty()) {
		const FunctionDecl* function = pending.back();
		pending.pop_back();

		std::vector<const Expression*> calls;
		collectCalls(*function->body, calls);
		for (const Expression* call: calls) {
			for (const FunctionDecl* callee: functionsCalledBy(*call)) {
				callees[function].push_back(callee);
				if (callees.try_emplace(callee).second) {
					pending.push_back(callee);
					if (scan(*callee)) {
						naming.insert(callee);
					}
					// A constexpr sampler a helper declares and names is one of this entry point's, as one the
					// entry point declares is, so the helper is emitted per entry point too.
					if (callee->body) {
						std::set<std::string> used;
						collectIdentifiers(*callee->body, used, false);
						if (reserveLocalSamplers(*callee->body, used, false)) {
							naming.insert(callee);
							declaring.push_back(callee);
						}
					}
				}
			}
		}
		if (!subscriptHelpers.empty() && hasIdxStmt(hasIdxStmt, function->body.get())) {
			for (const FunctionDecl* callee: subscriptHelpers) {
				callees[function].push_back(callee);
				if (callees.try_emplace(callee).second) {
					pending.push_back(callee);
					if (scan(*callee)) { naming.insert(callee); }
					if (callee->body) {
						std::set<std::string> used;
						collectIdentifiers(*callee->body, used, false);
						if (reserveLocalSamplers(*callee->body, used, false)) {
							naming.insert(callee);
							declaring.push_back(callee);
						}
					}
				}
			}
		}
	}
	for (bool grew = true; grew;) {
		grew = false;
		for (const auto& [function, called]: callees) {
			if (function != _entryPoint && !naming.count(function)
				&& std::any_of(called.begin(), called.end(), [&](const FunctionDecl* callee) {
					return naming.count(callee) > 0; })) {
				naming.insert(function);
				grew = true;
			}
		}
	}
	_samplerDependent.insert(naming.begin(), naming.end());

	// They go in the order the file declares them.
	for (const VariableDeclaration& global: _unit.globals) {
		if (named.count(&global)) {
			reserveSampler(global);
			_fileScopeSamplerBindings[global.name] = embeddedSamplerBinding(global);
		}
	}
	for (const FunctionDecl* helper: declaring) {
		std::set<std::string> used;
		collectIdentifiers(*helper->body, used, false);
		reserveLocalSamplers(*helper->body, used);
	}
}

Binding Emitter::embeddedSamplerBinding(const VariableDeclaration& declaration) const {
	const auto found = std::find_if(_embeddedSamplers.begin(), _embeddedSamplers.end(),
		[&](const EmbeddedSampler& other) { return other.state == *declaration.sampler; });

	Binding bound;
	bound.id = found->variable;
	bound.isPointer = true;
	bound.pointeeType = _types.samplerType();
	bound.storageClass = spirv::StorageClass::UniformConstant;
	bound.pointeeMsl = declaration.type;
	bound.unnormalizedSampler = !declaration.sampler->normalizedCoordinates;
	bound.embeddedSampler = true;
	return bound;
}

void Emitter::declareLocalSampler(const VariableDeclaration& declaration) {
	const bool reserved = std::any_of(_embeddedSamplers.begin(), _embeddedSamplers.end(),
		[&](const EmbeddedSampler& other) { return other.state == *declaration.sampler; });
	if (!reserved) {
		return;  // never named after its declaration, so Apple records no state for it
	}

	_bindings[declaration.name] = embeddedSamplerBinding(declaration);
}

// "texture2d<float> b = a;", "sampler t = s;" and the auto forms: the new name is the same
// descriptor as the one it was copied from, so nothing is declared and the reflection gains no
// entry. Only a name is accepted on the right, and the name is never rebound (emitAssignment),
// so there is no run-time choice among descriptors for the alias to stand for.
void Emitter::bindResourceAlias(const VariableDeclaration& declaration, const Type* declared) {
	const Expression& source = *declaration.initializer;
	const bool pointer = declared && declared->isPointer;
	const std::string what = std::string("the ") + (declared && declared->resource == ResourceKind::Sampler
		? "sampler" : "texture or sampler") + (pointer ? " pointer" : "") + " \"" + declaration.name + "\"";
	if (source.kind == ExpressionKind::Identifier && source.name == declaration.name && !source.fileScope) {
		throw CompileError(what + " is initialised from itself");
	}
	const std::optional<ResolvedResource> resolved = resourceOf(source);
	if (!resolved && source.kind != ExpressionKind::Identifier) {
		throw CompileError(what + " has to be initialised from the name of a texture or sampler"
			+ std::string(pointer ? ", its address, " : " ") + "or a call that returns one: a "
			"conditional, a member or an element would pick the descriptor at run time, which "
			"mslc does not lower");
	}
	const std::string from = source.kind == ExpressionKind::Identifier ? "\"" + source.name + "\""
		: resolved ? "a " + typeName(resolved->binding.pointeeMsl) : std::string("its initialiser");
	if (!resolved) {
		throw CompileError(what + " is initialised from " + from + ", which is not a texture or a sampler");
	}
	if (declared && !sameResourceType(resolved->binding.pointeeMsl, *declared)) {
		throw CompileError(what + " is initialised from " + from + ", which is not a " + typeName(*declared));
	}
	if (resolved->pointer != pointer) {
		throw CompileError(what + " is initialised from " + (resolved->pointer
			? "the address of a texture or sampler; it is not a pointer"
			: "a texture or sampler; it is a pointer, and takes its address"));
	}
	if (pointer && resolved->binding.embeddedSampler && !declared->isConst) {
		throw CompileError(what + " points at a constexpr sampler, which is const; it has to be a pointer to const");
	}
	Binding alias = resolved->binding;
	alias.resourcePointer = pointer;
	_bindings[declaration.name] = alias;
}

// A half texture is sampled as a float image, a short one as an int image and a
// ushort one as a uint image, and narrowed here, because the image type's sampled
// type has 32 bits whatever the texture's component is.
Id Emitter::texturePixels(const Binding& texture, Id sampled) {
	const ScalarKind element = texture.pointeeMsl.scalar;
	if (textureSampledKind(element) == element) {
		return sampled;
	}

	return convert(sampled, _builder.typeOf(sampled), _types.vector(element, 4));
}

// What sample and read return: a depth texture gives the first channel of the image's
// four, one float (or half), and any other texture its four narrowed channels.
Id Emitter::textureTexel(const Binding& texture, Id sampled) {
	if (!texture.pointeeMsl.isDepth) {
		return texturePixels(texture, sampled);
	}
	const Id depth = _builder.emitTyped(spirv::OpCompositeExtract, _types.scalar(ScalarKind::Float), { sampled, 0u });
	return texture.pointeeMsl.scalar == ScalarKind::Half
		? convert(depth, _types.scalar(ScalarKind::Float), _types.scalar(ScalarKind::Half)) : depth;
}

Binding Emitter::textureReceiver(const Expression& call) {
	const Expression& member = *call.left;
	const std::optional<ResolvedResource> resolved = resourceOf(*member.left);
	if (!resolved && member.left->kind != ExpressionKind::Identifier) {
		throw CompileError("the receiver of \"." + member.memberName + "\" has to be a texture parameter");
	}
	if (!resolved || !resolved->binding.pointeeMsl.isTexture()) {
		throw CompileError((member.left->kind == ExpressionKind::Identifier ? "\"" + member.left->name + "\""
			: std::string("the receiver")) + " is not a texture parameter, so "
			"\"." + member.memberName + "\" is not a call mslc lowers");
	}
	if (resolved->pointer != member.isArrow) {
		throw CompileError(member.isArrow
			? "\"->" + member.memberName + "\" is called on a texture, not on a pointer to one; use \".\""
			: "\"." + member.memberName + "\" is called on a pointer to a texture; use \"->\"");
	}

	return resolved->binding;
}

// The descriptor an expression names, resolved while compiling, as a texture local is: a texture or sampler
// name, *p (p[0]) for a pointer to one, &t, and a call to a helper that returns a texture or sampler pointer
// or reference, resolved from its return. Nothing for any other expression; an expression that is about a
// texture but cannot be resolved this way is refused, so that no descriptor is chosen at run time.
std::optional<Emitter::ResolvedResource> Emitter::resourceOf(const Expression& expression, unsigned depth) {
	if (depth > 16) {
		throw CompileError("a texture or sampler is reached through calls nested more than 16 deep");
	}
	switch (expression.kind) {
		case ExpressionKind::Identifier: {
			const Binding* binding = findResourceBinding(expression.name, expression.fileScope);
			if (!binding || binding->pointeeMsl.resource == ResourceKind::None) {
				return std::nullopt;
			}
			return ResolvedResource{ *binding, binding->resourcePointer };
		}
		case ExpressionKind::Index: {
			std::optional<ResolvedResource> inner = expression.left ? resourceOf(*expression.left, depth) : std::nullopt;
			if (!inner) {
				return std::nullopt;
			}
			const std::optional<int64_t> index = expression.arguments.empty() ? std::nullopt
				: literalIndex(*expression.arguments.front());
			if (!inner->pointer || !index || *index != 0) {
				throw CompileError(inner->pointer
					? "a pointer to a texture or sampler is dereferenced (*p or p[0]) only, not indexed"
					: "a texture or sampler is dereferenced or indexed, and it is not a pointer");
			}
			inner->pointer = false;
			return inner;
		}
		case ExpressionKind::Unary: {
			if (expression.unaryOperator != UnaryOperator::AddressOf || !expression.left) {
				return std::nullopt;
			}
			std::optional<ResolvedResource> inner = resourceOf(*expression.left, depth);
			if (!inner) {
				return std::nullopt;
			}
			if (inner->pointer) {
				throw CompileError("the address of a pointer to a texture or sampler is not lowered");
			}
			inner->pointer = true;
			return inner;
		}
		case ExpressionKind::Call:
			return resourceCall(expression, depth);
		default:
			return std::nullopt;
	}
}

// A call to a helper that returns a texture or sampler pointer or reference ("thread T* addressOf(thread T& r)
// { return &r; }"): there is no such value at run time, so the helper's one return statement is resolved with
// its parameters bound to the arguments' descriptors, and the helper is not emitted.
std::optional<Emitter::ResolvedResource> Emitter::resourceCall(const Expression& call, unsigned depth) {
	if (!call.left || call.left->kind != ExpressionKind::Identifier) {
		return std::nullopt;
	}
	const std::vector<HelperFunction*> named = helpersCalled(call);
	const auto returnsResource = [](const HelperFunction* helper) {
		return helper->definition->returnType.resource != ResourceKind::None;
	};
	if (std::none_of(named.begin(), named.end(), returnsResource)) {
		return std::nullopt;
	}
	const FunctionDecl& function = *named.front()->definition;
	const std::string quoted = "\"" + function.name + "\"";
	if (named.size() != 1) {
		throw CompileError(quoted + " is overloaded and returns a texture or sampler, which mslc resolves "
			"for a function of one signature only");
	}
	const Statement* body = function.body.get();
	while (body && body->kind == StatementKind::Compound && body->children.size() == 1) {
		body = body->children.front().get();
	}
	if (!body || body->kind != StatementKind::Return || !body->expression) {
		throw CompileError(quoted + " returns a texture or sampler pointer or reference, which mslc resolves "
			"while compiling from a body that is one return statement");
	}
	if (function.parameters.size() != call.arguments.size()) {
		throw CompileError(quoted + " takes " + std::to_string(function.parameters.size()) + " arguments, and "
			"the call gives " + std::to_string(call.arguments.size()));
	}

	std::vector<std::pair<std::string, Binding>> parameters;
	for (size_t i = 0; i < function.parameters.size(); ++i) {
		const Parameter& parameter = function.parameters[i];
		if (parameter.type.resource == ResourceKind::None) {
			throw CompileError(quoted + " returns a texture or sampler and takes \"" + parameter.name
				+ "\", which is not one; mslc resolves such a function from texture and sampler arguments only");
		}
		const std::optional<ResolvedResource> argument = resourceOf(*call.arguments[i], depth + 1);
		if (!argument || !sameResourceType(argument->binding.pointeeMsl, parameter.type)
			|| argument->pointer != parameter.type.isPointer) {
			throw CompileError("argument " + std::to_string(i + 1) + " of the call to " + quoted + " is not a "
				+ typeName(parameter.type) + (parameter.type.isPointer ? " pointer" : ""));
		}
		if (argument->binding.embeddedSampler && (parameter.type.isPointer || parameter.isReference)
			&& !parameter.type.isConst) {
			throw CompileError("argument " + std::to_string(i + 1) + " of the call to " + quoted + " is a constexpr "
				"sampler, which is const, and parameter \"" + parameter.name + "\" is a pointer or reference that "
				"is not to const");
		}
		Binding bound = argument->binding;
		bound.resourcePointer = parameter.type.isPointer;
		parameters.emplace_back(parameter.name, bound);
	}

	std::optional<ResolvedResource> returned;
	{
		const BindingScope scope(_bindings);
		_bindings.clear();
		for (const auto& [name, binding]: parameters) {
			_bindings[name] = binding;
		}
		returned = resourceOf(*body->expression, depth + 1);
	}
	const Type& declared = function.returnType;
	if (!returned || returned->pointer != declared.isPointer
		|| !sameResourceType(returned->binding.pointeeMsl, declared)) {
		throw CompileError(quoted + " returns a " + typeName(declared) + (declared.isPointer ? " pointer" : "")
			+ ", which mslc resolves only when its return statement names one of its texture or sampler "
			"parameters, its address, or such a call");
	}
	return returned;
}

} // namespace mslc
