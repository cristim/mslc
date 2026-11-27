#include "sema/emitter.h"

namespace mslc {

// A name, or an element of a local array of scalars: what a reference binds without question.
bool Emitter::isWholeVariable(const Expression& expression) const {
	const Expression* root = &expression;
	while (root->kind == ExpressionKind::Index) {
		root = root->left.get();
	}
	if (root->kind != ExpressionKind::Identifier) {
		return false;
	}
	if (root == &expression) {
		return true;
	}
	const Binding* binding = findBinding(*root);
	return binding && binding->pointeeMsl.arrayLength
		&& binding->pointeeMsl.vectorWidth <= 1 && binding->pointeeMsl.matrixColumns == 0
		&& binding->pointeeMsl.namedType.empty();
}

// A name, an element or a member of one, with nothing in it that evaluating twice would repeat.
bool Emitter::isPlainVariable(const Expression& expression) const {
	switch (expression.kind) {
		case ExpressionKind::Identifier: return true;
		case ExpressionKind::Member: return isPlainVariable(*expression.left);
		case ExpressionKind::Index:
			return isPlainVariable(*expression.left) && expression.arguments.size() == 1
				&& (expression.arguments[0]->kind == ExpressionKind::IntLiteral
					|| expression.arguments[0]->kind == ExpressionKind::Identifier);
		default: return false;
	}
}

Emitter::OverloadArgument Emitter::classifyArgument(const Expression& expression) {
	OverloadArgument argument;
	argument.expression = &expression;
	const bool place = (expression.kind == ExpressionKind::Identifier
		|| expression.kind == ExpressionKind::Index || expression.kind == ExpressionKind::Member)
		&& !isComputedValue(expression);
	argument.kind = place ? OverloadArgument::Kind::Variable : OverloadArgument::Kind::Value;
	if (expression.kind == ExpressionKind::Identifier) {
		if (const Binding* binding = findResourceBinding(expression.name, expression.fileScope)) {
			const bool array = binding->isPointer && binding->pointeeMsl.arrayLength
				&& (binding->storageClass == spirv::StorageClass::Function
					|| binding->storageClass == spirv::StorageClass::Workgroup)
				&& !binding->isAddress;
			if (binding->pointeeMsl.resource != ResourceKind::None) {
				argument.kind = OverloadArgument::Kind::Resource;
			} else if (array) {
				argument.kind = OverloadArgument::Kind::Array;
			}
			argument.binding = binding;
			argument.pointerLike = argument.kind == OverloadArgument::Kind::Variable
				&& (binding->pointeeMsl.isPointer || binding->pointeeMsl.arrayLength || binding->isBuffer);
		}
	} else if (expression.kind == ExpressionKind::Unary && expression.unaryOperator == UnaryOperator::AddressOf) {
		argument.pointerLike = true;
	}
	if (argument.kind == OverloadArgument::Kind::Variable) {
		const Expression& root = storeRoot(expression);
		const Binding* local = root.kind == ExpressionKind::Identifier && !root.fileScope
			? findResourceBinding(root.name) : nullptr;
		argument.inConstant = inConstantSpace(expression);
		argument.inThread = local && !local->isBuffer && local->bufferPointeeType == InvalidId
			&& local->storageClass == spirv::StorageClass::Function && !argument.inConstant;
		argument.changeable = argument.inThread && !local->readOnly;
	}
	return argument;
}

// How an argument of one type reaches a parameter, per the ranks above. A vector, a matrix or a
// struct converts to nothing else; an enum is an int that converts to no enum but its own.
Emitter::ConversionCost Emitter::conversionCost(const OverloadArgument& argument, const Parameter& parameter) {
	const Type& type = parameter.type;
	const auto exact = [](int qualification) { return ConversionCost { kExact, qualification }; };
	const ConversionCost none;

	if (type.resource != ResourceKind::None) {
		const Binding* binding = argument.kind == OverloadArgument::Kind::Resource ? argument.binding : nullptr;
		return binding && sameResourceType(binding->pointeeMsl, type) ? exact(-1) : none;
	}
	if (type.isPointer || type.arrayLength) {
		const bool buffer = argument.kind == OverloadArgument::Kind::Variable && !argument.expression->fileScope
			&& argument.binding && argument.binding->isPointer && argument.binding->isBuffer
			&& argument.binding->bufferPointeeType != InvalidId;
		if (buffer) {
			if (!type.isPointer || type.arrayLength || !type.innerArrayLengths.empty()
				|| argument.binding->pointeeMsl.addressSpace != type.addressSpace) {
				return none;
			}
			const Type& given = argument.binding->pointeeMsl;
			const bool canWrite = type.addressSpace == AddressSpace::Device && !type.isConst;
			return given.isAtomic != type.isAtomic || given.enumName != type.enumName
				|| argument.binding->bufferPointeeType != bufferParamTypes(type).second
				|| (argument.binding->readOnly && canWrite)
				? none : exact(type.isConst && !argument.binding->readOnly ? 1 : 0);
		}
		if (argument.kind != OverloadArgument::Kind::Array) {
			if (argument.expression->kind == ExpressionKind::IntLiteral && argument.expression->intValue == 0) {
				throw CompileError("a literal 0 chooses among overloads one of which takes a pointer; the "
					"null pointer conversion is not lowered yet");
			}
			if (argument.pointerLike) {
				throw CompileError("a pointer argument other than a local array or a whole buffer (&x, a + n "
					"or a pointer into a buffer) chooses among overloads, which is not lowered yet");
			}
			return none;
		}
		// A local array reaches a pointer in its own address space: thread, or threadgroup for one in
		// Workgroup storage, and no other.
		const bool threadgroupArray = argument.binding->storageClass == spirv::StorageClass::Workgroup;
		if ((type.addressSpace == AddressSpace::Threadgroup) != threadgroupArray
			|| type.addressSpace == AddressSpace::Device || type.addressSpace == AddressSpace::Constant) {
			return none;
		}
		return !pointerFit(*argument.binding, type).matches
			|| (argument.binding->readOnly && !type.isConst)
			? none : exact(type.isConst && !argument.binding->readOnly ? 1 : 0);
	}
	if (argument.kind == OverloadArgument::Kind::Array || argument.kind == OverloadArgument::Kind::Resource
		|| argument.pointerLike) {
		if (declaredTypeOf(type) == _boolType) {
			throw CompileError("a pointer or array argument chooses among overloads one of which takes a bool; "
				"the pointer to bool conversion is not lowered yet");
		}
		return none;
	}

	const Id to = declaredTypeOf(type);
	const Id from = _builder.typeOf(argument.value);
	const bool enumArgument = argument.tag && !argument.tag->empty();
	if (parameter.isReference && type.addressSpace == AddressSpace::Constant) {
		// It binds a constant-space place of exactly its type, and nothing else (no conversion
		// makes a temporary in another address space).
		if (argument.kind == OverloadArgument::Kind::Variable && argument.inConstant
			&& isVectorComponent(*argument.expression)) {
			// Apple finds the candidate viable and then errors if it wins; dropping it here could
			// silently pick another overload.
			throw CompileError("an argument that is a component of a constant vector chooses among overloads "
				"one of which takes a constant reference; Apple binds it and errors, which is not modelled");
		}
		return argument.kind == OverloadArgument::Kind::Variable && argument.inConstant && from == to
			&& type.enumName == (argument.tag ? *argument.tag : std::string()) ? exact(1) : none;
	}
	if (enumArgument && from != _intType) {
		throw CompileError("an enum whose underlying type is not int chooses among overloads, which is not "
			"lowered yet (it promotes to its own type)");
	}
	if (parameter.isMutableReference()) {
		// A reference names its argument as it is: same type, a non-const local of this function.
		return argument.changeable && from == to
			&& type.enumName == (argument.tag ? *argument.tag : std::string()) ? exact(0) : none;
	}
	const int qualification = parameter.isReference ? 1 : -1;
	// A reference to thread memory cannot bind a variable of another address space.
	if (parameter.isReference && argument.kind == OverloadArgument::Kind::Variable && !argument.inThread
		&& from == to && type.enumName == (argument.tag ? *argument.tag : std::string())) {
		return none;
	}
	if (!type.enumName.empty()) {
		return enumArgument && *argument.tag == type.enumName ? exact(qualification) : none;
	}
	if (!enumArgument && from == to) {
		return exact(qualification);
	}
	// A struct read from a buffer has the buffer's layout, a type of its own: same-named is the same struct.
	if (!type.namedType.empty()) {
		const std::string* named = _types.structNameOf(from, nullptr);
		return named && *named == type.namedType
			&& !(parameter.isReference && argument.kind == OverloadArgument::Kind::Variable && !argument.inThread)
			? exact(qualification) : none;
	}
	const std::optional<ScalarKind> fromKind = _types.scalarKindOf(from);
	if (!fromKind) {
		return none;
	}
	if (enumArgument && *argument.tag == "metal::component") {
		return none;
	}
	if (_types.vectorWidth(to) > 1) {
		return _types.scalarKindOf(_types.componentOf(to)) ? ConversionCost { kSplat, -1 } : none;
	}
	const std::optional<ScalarKind> toKind = _types.scalarKindOf(to);
	if (!toKind) {
		return none;
	}
	const bool promotes = enumArgument || *fromKind == ScalarKind::Bool || *fromKind == ScalarKind::Char
		|| *fromKind == ScalarKind::UChar || *fromKind == ScalarKind::Short || *fromKind == ScalarKind::UShort;
	return { promotes && *toKind == ScalarKind::Int ? kPromotion : kConversion, -1 };
}

// An array parameter's own length is not part of its type (it decays), but the
// lengths inside it are: float m[2][3] takes an array of rows of three.
Emitter::PointerFit Emitter::pointerFit(const Binding& array, const Type& parameter) {
	PointerFit fit;
	fit.element = array.pointeeMsl;
	fit.element.dropOuterDimension();
	fit.shape = fit.element.innerArrayLengths;
	if (fit.element.arrayLength) {
		fit.shape.insert(fit.shape.begin(), *fit.element.arrayLength);
	}
	fit.element.arrayLength.reset();
	fit.element.innerArrayLengths.clear();
	fit.wanted = parameter;
	fit.wanted.isPointer = false;
	fit.wantedShape = fit.wanted.innerArrayLengths;
	fit.wanted.innerArrayLengths.clear();
	fit.matches = declaredTypeOf(fit.element) == declaredTypeOf(fit.wanted)
		&& fit.element.namedType == fit.wanted.namedType && fit.shape == fit.wantedShape;
	return fit;
}

// A call is an edge of the call graph. validateHelpers sees the edges whose callee a name
// identifies; the others are checked here, against the edges emitted so far.
void Emitter::noteCall(const FunctionDecl& callee) {
	_functionCallees[_helper ? _helper : _entryPoint].insert(&callee);
	if (!_helper) {
		return;
	}
	std::vector<const FunctionDecl*> pending { &callee };
	std::set<const FunctionDecl*> seen;
	while (!pending.empty()) {
		const FunctionDecl* at = pending.back();
		pending.pop_back();
		if (at == _helper) {
			throw CompileError("recursive call: \"" + _helper->name + "(" + parameterSignature(*_helper)
				+ ")\" reaches itself through \"" + callee.name + "(" + parameterSignature(callee)
				+ ")\"; SPIR-V for Vulkan forbids recursion (Apple's compiler accepts it)");
		}
		if (seen.insert(at).second) {
			const auto edges = _callEdges.find(at);
			if (edges != _callEdges.end()) {
				pending.insert(pending.end(), edges->second.begin(), edges->second.end());
			}
		}
	}
	_callEdges[_helper].insert(&callee);
}

bool Emitter::returnsVoid(const FunctionDecl& function) const {
	return function.returnType.scalar == ScalarKind::Void && function.returnType.namedType.empty();
}

// A texture or sampler parameter is a pointer to the UniformConstant variable
// the caller's own parameter or sampler is, because Vulkan SPIR-V passes an
// opaque object by reference. Any other parameter is the value it names.
Id Emitter::helperParameterType(const Type& type) {
	if (type.resource == ResourceKind::None) {
		return declaredTypeOf(type);
	}

	return _types.pointer(spirv::StorageClass::UniformConstant, resourcePointee(type));
}

Id Emitter::resourcePointee(const Type& type) {
	if (type.isStorageTexture()) {
		return _types.storageImage(type.resource, type.textureAccess, textureSampledKind(type.scalar));
	}
	return type.isTexture() ? _types.image(type.isFetchedCube() ? ResourceKind::Texture2DArray : type.resource,
		textureSampledKind(type.scalar), type.isDepth, type.isMultisampled) : _types.samplerType();
}

// The variable behind a texture or sampler argument: it has to be a name for a
// texture or sampler of the same kind as the parameter, since there is no
// other way to hold one, and Apple finds no function for any other call.
Id Emitter::resourceArgument(const Expression& argument, const Parameter& parameter,
	const std::string& function) {
	const std::string what = "the argument for parameter \"" + parameter.name + "\" of \"" + function + "\"";
	if (argument.kind != ExpressionKind::Identifier) {
		throw CompileError(what + " has to name a texture or sampler parameter, a sampler declared "
			"in the shader, or a texture or sampler parameter of the calling helper");
	}

	const Binding* binding = findResourceBinding(argument.name, argument.fileScope);
	const Type& wanted = parameter.type;
	if (!binding || !sameResourceType(binding->pointeeMsl, wanted)) {
		throw CompileError(what + " is \"" + argument.name + "\", which is not a "
			+ typeName(wanted));
	}

	// The helper samples with an implicit lod, which Vulkan refuses through an
	// unnormalized sampler, and the helper is one function whatever it is given.
	if (binding->unnormalizedSampler) {
		throw CompileError(what + " is a coord::pixel sampler, which a helper function does not take: "
			"Vulkan forbids an implicit-lod lookup through it");
	}

	return binding->id;
}

// The array a pointer parameter is bound to inside the helper: the caller's array of
// the given length, of elements the parameter points at.
Type boundArray(const Type& parameter, uint32_t length) {
	Type array = parameter;
	array.isPointer = false;
	array.addressSpace = AddressSpace::None;
	array.arrayLength = length;
	return array;
}

// The id the function is called by in the scope being emitted, with its type
// declared and its body queued the first time. A function this unit only
// declares is one id for the whole module, so it is declared once.
Id Emitter::helperFunctionId(HelperFunction& helper) {
	const FunctionDecl& definition = *helper.definition;
	if (helper.type == InvalidId) {
		helper.returnType = returnsVoid(definition) ? _voidType : declaredTypeOf(definition.returnType);
		std::vector<Id> signature { helper.returnType };
		for (size_t i = 0; i < definition.parameters.size(); ++i) {
			const Parameter& parameter = definition.parameters[i];
			if (i < helper.threadgroupVariables.size() && helper.threadgroupVariables[i] != InvalidId) {
				helper.parameterTypes.push_back(InvalidId);
				continue;
			}
			if (parameter.type.isPointer
				&& (parameter.type.addressSpace == AddressSpace::Device
					|| parameter.type.addressSpace == AddressSpace::Constant)) {
				helper.parameterTypes.push_back(
					_types.bufferPointer(bufferParamTypes(parameter.type).second));
			} else {
				helper.parameterTypes.push_back(parameter.type.isPointer
					? _types.pointer(spirv::StorageClass::Function,
						arrayTypeOf(boundArray(parameter.type, helper.arrayLengths[i])))
					: parameter.isMutableReference()
					? _types.pointer(spirv::StorageClass::Function, declaredTypeOf(parameter.type))
					: helperParameterType(parameter.type));
			}
			signature.push_back(helper.parameterTypes.back());
		}

		// One OpTypeFunction per distinct signature: a duplicate fails validation.
		const auto slot = _functionTypes.emplace(signature, InvalidId);
		if (slot.second) {
			slot.first->second = _builder.emitDecl(spirv::OpTypeFunction, signature);
		}

		helper.type = slot.first->second;
	}

	const auto slot = helper.ids.emplace(helperScope(helper), InvalidId);
	if (slot.second) {
		slot.first->second = _builder.nextId();
		_helperQueue.push_back(&helper);
	}

	return slot.first->second;
}

// A helper with a body is emitted once per stage that reaches it, or once per entry point
// when it names a file-scope sampler or declares a constexpr one, which is that entry point's own variable. One the unit
// only declares, and one a library exports, belong to no stage.
std::pair<Stage, const FunctionDecl*> Emitter::helperScope(const HelperFunction& helper) const {
	if (!helper.definition->body) {
		return { Stage::None, nullptr };
	}
	return { _entryPoint->stage, _samplerDependent.count(helper.definition) ? _entryPoint : nullptr };
}

// Arguments are evaluated left to right and converted to the parameter types.
// Resource arguments retain their caller's identity, including writes.
Id Emitter::emitHelperCall(const Expression& call, HelperFunction& generic, const Expression* receiver,
	const std::vector<Id>& prepared) {
	const FunctionDecl& definition = *generic.definition;
	noteCall(definition);
	// The scan before emission does not follow argument-dependent lookup or template instances, so a
	// caller it did not mark would share one body between entry points and call the first one's sampler. With
	// one entry point there is no other to share it with.
	if (_helper && _entryPoints.size() > 1 && _samplerDependent.count(&definition)
		&& !_samplerDependent.count(_helper)) {
		throw CompileError("\"" + _helper->name + "\" reaches \"" + definition.name + "\", which names a "
			"file-scope sampler or declares a constexpr one, only through argument-dependent lookup or a template instance; pass the sampler "
			"as a parameter or call it by a qualified name");
	}
	std::vector<Id> arrays(definition.parameters.size(), InvalidId);
	HelperFunction& helper = pointerSpecialisation(call, generic, receiver ? 1 : 0, arrays);

	std::vector<uint32_t> operands { helperFunctionId(helper) };
	std::vector<std::pair<Id, Id>> copyOuts;
	std::vector<std::optional<StdlibOperand>> argumentOperands(call.arguments.size());
	const auto stdlibArgumentOperand = [&](Id type) -> std::optional<StdlibOperand> {
		const uint32_t width = _types.vectorWidth(type);
		if (const auto scalar = _types.scalarKindOf(width > 1 ? _types.componentOf(type) : type)) {
			return StdlibOperand{*scalar, width};
		}
		return std::nullopt;
	};
	const size_t first = receiver ? 1 : 0;
	rejectAliasedReferenceArguments(call, definition, first);
	if (first) {
		if (!validMemberReceiver(definition, *receiver)) {
			throw CompileError(definition.parameters[0].type.addressSpace == AddressSpace::Constant
				? "the member function \"" + definition.name + "\" is marked constant, and the object is not in the constant address space"
				: "the member function \"" + definition.name + "\" is called on an object in device, constant or threadgroup memory, "
					"which Apple rejects (mark it constant, or copy the object to a local first)");
		}
		const Id object = _pendingObject != InvalidId ? _pendingObject : emitExpression(*receiver);
		_pendingObject = InvalidId;
		operands.push_back(convertImplicit(object, helper.parameterTypes[0]));
	}
	for (size_t i = 0; i < call.arguments.size(); ++i) {
		const Parameter& parameter = definition.parameters[first + i];
		if (parameter.type.resource != ResourceKind::None) {
			operands.push_back(resourceArgument(*call.arguments[i], parameter, definition.name));
			continue;
		}

		if (parameter.type.isPointer) {
			if (helper.parameterTypes[first + i] != InvalidId) {
				operands.push_back(arrays[first + i]);
			}
			continue;
		}

		if (parameter.isReference && parameter.type.addressSpace == AddressSpace::Constant) {
			const Id value = constantReferenceArgument(*call.arguments[i], parameter, definition.name, i + 1,
				prepared.empty() ? InvalidId : prepared[i]);
			if (parameter.type.enumName.empty()) {
				argumentOperands[i] = stdlibArgumentOperand(_builder.typeOf(value));
			}
			operands.push_back(convertImplicit(value, helper.parameterTypes[first + i]));
			continue;
		}

		if (parameter.isMutableReference()) {
			operands.push_back(referenceArgument(*call.arguments[i], parameter, definition.name, i + 1, copyOuts));
			// An enum-typed parameter is an exact match for an enum argument, which beats the
			// int promotion a library signature would need; leaving no operand reads as no
			// match, same as any other type the library's signatures do not model.
			if (parameter.type.enumName.empty()) {
				argumentOperands[i] = stdlibArgumentOperand(valueTypeAt(operands.back()));
			}
			continue;
		}

		requireEnumValue(parameter.type.enumName, *call.arguments[i],
			"argument " + std::to_string(i + 1) + " of the call to \"" + definition.name + "\"");
		const Id value = prepared.empty() ? emitExpression(*call.arguments[i]) : prepared[i];
		if (parameter.type.enumName.empty()) {
			argumentOperands[i] = stdlibArgumentOperand(_builder.typeOf(value));
		}
		try {
			operands.push_back(convertImplicit(value, helper.parameterTypes[first + i]));
		} catch (const CompileError& error) {
			throw CompileError("argument " + std::to_string(i + 1) + " of the call to \""
				+ definition.name + "\" cannot be converted to the parameter type "
				+ typeName(definition.parameters[first + i].type) + ": " + error.what());
		}
	}

	// The standard library's overloads compete with this function where its names are
	// visible beside it. Where its parameters are one of theirs, Apple never calls it
	// alone (ambiguous, or another library overload wins); where the arguments are exactly
	// one of theirs, that one wins or ties.
	std::set<std::string> stdlibBeside = call.left->stdlibBeside;
	if (!receiver && definition.name.compare(0, stdlibPrefix.size(), stdlibPrefix) == 0 && !isMemberName(definition.name)) {
		stdlibBeside.insert(definition.name.substr(stdlibPrefix.size()));
	}
	if (!stdlibBeside.empty()) {
		std::vector<std::optional<StdlibOperand>> parameterOperands;
		for (const Parameter& parameter: definition.parameters) {
			parameterOperands.push_back(stdlibOperand(parameter));
		}
		for (const std::string& stdlibName: stdlibBeside) {
			if (isStdlibSignature(stdlibName, parameterOperands) || isStdlibSignature(stdlibName, argumentOperands)) {
				throw CompileError("the call to \"" + definition.name + "\" also matches the standard library's \""
					+ stdlibName + "\", which is visible beside it; Apple resolves the two by overload, which mslc "
					"does not model");
			}
		}
	}

	const Id result = _builder.emitTyped(spirv::OpFunctionCall, helper.returnType, operands);
	for (const auto& [place, temporary]: copyOuts) {
		const Id type = valueTypeAt(place);
		_builder.emit(spirv::OpStore, { place, loadFrom(temporary, type) });
	}
	return result;
}

// A device or constant pointer argument names a whole buffer: an entry point's
// own buffer parameter, or the pointer parameter of the calling helper. Apple
// also takes &b[i] and b + n, pointers into the middle of one, and pointer
// values copied or compared; none of those is a name the helper's parameter
// can hold, so each is refused.
Id Emitter::bufferPointerArgument(const Expression& argument, const Parameter& parameter,
	const std::string& what) {
	const std::string space = addressSpaceName(parameter.type.addressSpace);
	if (argument.kind != ExpressionKind::Identifier) {
		throw CompileError(what + " has to name a whole buffer, because parameter \"" + parameter.name
			+ "\" is a pointer in the " + space + " address space, and &b[i] and other pointers "
			"into the middle of one are not lowered yet");
	}
	if (argument.fileScope) {
		// A file-scope object is a constant, a sampler or a struct, never a buffer; without
		// this a "::data" spelled past a kernel buffer of the same name would silently bind
		// the buffer, because this path never calls emitExpression's file-scope correction.
		throw CompileError(what + " is \"" + argument.name + "\", which names a file-scope object, "
			+ "and a file-scope object is not a buffer; parameter \"" + parameter.name + "\" is a "
			+ "pointer in the " + space + " address space and takes one");
	}
	const auto binding = _bindings.find(argument.name);
	if (binding == _bindings.end() || !binding->second.isPointer || !binding->second.isBuffer
		|| binding->second.bufferPointeeType == InvalidId) {
		throw CompileError(what + " is \"" + argument.name + "\", which is not a buffer; parameter \""
			+ parameter.name + "\" is a pointer in the " + space + " address space and takes one");
	}
	if (binding->second.pointeeMsl.addressSpace != parameter.type.addressSpace) {
		throw CompileError(what + " is in the "
			+ addressSpaceName(binding->second.pointeeMsl.addressSpace) + " address space, but "
			+ "parameter \"" + parameter.name + "\" takes a pointer in the " + space
			+ " one, and Metal does not convert between address spaces");
	}
	// The pointee has to be the type the parameter names, as the source spells it and
	// as it is laid out: an atomic_uint buffer and a uint one share a block struct, an
	// enum and its underlying type share the scalar, and a float3 buffer and a
	// packed_float3 one share a vector type but stride 16 and 12. Ordinary loads and
	// stores through the parameter would read or write the wrong thing, so only a
	// difference in const qualification is allowed.
	const Type& given = binding->second.pointeeMsl;
	if (given.isAtomic != parameter.type.isAtomic || given.enumName != parameter.type.enumName
		|| binding->second.bufferPointeeType != bufferParamTypes(parameter.type).second) {
		throw CompileError(what + " is a buffer of " + typeName(given) + ", and parameter \""
			+ parameter.name + "\" points at " + typeName(parameter.type)
			+ ", which is not the same element type");
	}
	// A constant-space pointer can never write, so a read-only buffer binds it;
	// only a device pointer that is not const could change a const buffer.
	const bool canWrite = parameter.type.addressSpace == AddressSpace::Device
		&& !parameter.type.isConst;
	if (binding->second.readOnly && canWrite) {
		throw CompileError(what + " is const, and parameter \"" + parameter.name
			+ "\" is a pointer that can change it");
	}
	return bufferBase(binding->second);
}

// A thread pointer parameter takes a local array of its element type. Vulkan
// without VariablePointers cannot index a pointer to an element by a run-time
// value, so the helper is emitted once per list of array lengths and its
// parameter is a pointer to the whole array, which p[i] indexes with an access
// chain. The helper then reads and writes the caller's own storage. Apple also
// takes &x, a + 1 and a pointer reassigned in the body; none of those is an
// array the helper can name, so each is refused.
Emitter::HelperFunction& Emitter::pointerSpecialisation(const Expression& call, HelperFunction& generic,
	size_t first, std::vector<Id>& arrays) {
	const FunctionDecl& definition = *generic.definition;
	std::vector<uint32_t> lengths(definition.parameters.size(), 0);
	std::vector<Id> variables(definition.parameters.size(), InvalidId);
	bool any = false;
	for (size_t i = 0; i < call.arguments.size(); ++i) {
		const Parameter& parameter = definition.parameters[first + i];
		if (!parameter.type.isPointer) {
			continue;
		}
		any = true;

		const std::string what = "argument " + std::to_string(i + 1) + " of the call to \""
			+ definition.name + "\"";
		const Expression& argument = *call.arguments[i];

		// A device or constant pointer takes a whole buffer: an entry point's
		// own buffer parameter, or the pointer parameter of the calling helper.
		if (parameter.type.addressSpace == AddressSpace::Device
			|| parameter.type.addressSpace == AddressSpace::Constant) {
			arrays[first + i] = bufferPointerArgument(argument, parameter, what);
			continue;
		}

		// A threadgroup pointer takes a threadgroup local array in the same way, in Workgroup storage.
		const bool threadgroup = parameter.type.addressSpace == AddressSpace::Threadgroup;
		const spirv::StorageClassValue storage = threadgroup
			? spirv::StorageClass::Workgroup : spirv::StorageClass::Function;
		const Binding* binding = findBinding(argument);
		if (!binding || !binding->isPointer || !binding->pointeeMsl.arrayLength
			|| binding->storageClass != storage || binding->isAddress) {
			throw CompileError(what + " has to name a " + (threadgroup ? "threadgroup " : "") + "local array "
				"(or a pointer parameter of the calling helper), since parameter \"" + parameter.name
				+ "\" is a " + (threadgroup ? "threadgroup " : "") + "pointer to its elements; &x, a + n and "
				"pointers into a buffer are not lowered yet");
		}
		// A kernel's [[threadgroup(n)]] argument is sized when the pipeline is made, so no helper
		// specialised on a length can take it.
		if (*binding->pointeeMsl.arrayLength == UINT32_MAX) {
			throw CompileError(what + " is a threadgroup argument of the kernel, whose length the pipeline "
				"sets, and a threadgroup pointer parameter takes only a threadgroup local array yet");
		}

		const PointerFit fit = pointerFit(*binding, parameter.type);
		const Type& element = fit.element;
		const Type& wanted = fit.wanted;
		const std::vector<uint32_t>& shape = fit.shape;
		const std::vector<uint32_t>& wantedShape = fit.wantedShape;
		if (!fit.matches) {
			const auto spelled = [&](const Type& base, const std::vector<uint32_t>& lengths) {
				std::string name = typeName(base);
				for (const uint32_t length: lengths) { name += "[" + std::to_string(length) + "]"; }
				return name;
			};
			throw CompileError(what + " is an array of " + spelled(element, shape) + ", and parameter \""
				+ parameter.name + "\" points at " + spelled(wanted, wantedShape));
		}
		if (binding->readOnly && !parameter.type.isConst) {
			throw CompileError(what + " is const, and parameter \"" + parameter.name
				+ "\" is a pointer that can change it");
		}

		lengths[first + i] = *binding->pointeeMsl.arrayLength;
		arrays[first + i] = binding->id;
		if (threadgroup) {
			variables[first + i] = binding->id;
		}
	}

	if (!any) {
		return generic;
	}

	std::vector<uint32_t> key = lengths;
	key.insert(key.end(), variables.begin(), variables.end());
	const auto slot = _pointerSpecialisations.emplace(std::make_pair(generic.definition, key), generic);
	if (slot.second) {
		slot.first->second.arrayLengths = lengths;
		slot.first->second.threadgroupVariables = variables;
	}
	return slot.first->second;
}

// An element, a member, or a reference local bound to one is passed through a
// temporary copied back after the call, so a second reference argument naming
// the same storage would have its write overwritten by the stale copy. Two
// bare variables are the same pointer and alias as C++ does.
void Emitter::rejectAliasedReferenceArguments(const Expression& call, const FunctionDecl& definition,
	size_t first) {
	struct Passed { const Expression* rootExpr; std::string root; bool temporary; std::vector<AccessStep> steps; };
	std::vector<Passed> passed;
	for (size_t i = 0; i < call.arguments.size(); ++i) {
		if (!definition.parameters[first + i].isMutableReference()) {
			continue;
		}
		const Expression& argument = *call.arguments[i];
		if (argument.kind != ExpressionKind::Identifier && argument.kind != ExpressionKind::Index
			&& argument.kind != ExpressionKind::Member) {
			continue;
		}
		const Expression& root = storeRoot(argument);
		if (root.kind != ExpressionKind::Identifier || isComputedValue(argument)) {
			continue;
		}
		const Binding* bound = findBinding(root);
		std::string rootName;
		std::vector<AccessStep> steps;
		accessChain(argument, rootName, steps);
		passed.push_back({ &root, root.name, argument.kind != ExpressionKind::Identifier
			|| (bound && bound->isAddress), std::move(steps) });
	}

	// A copied-in element or member is stale for anything else that reads or writes the same
	// storage during the call: a pointer parameter handed the whole array, or an argument that
	// writes (assignment, a call of a helper) while naming that storage.
	const bool copiedIn = std::any_of(passed.begin(), passed.end(), [](const Passed& p) { return p.temporary; });
	if (copiedIn) {
		const auto mayWrite = [&](const auto& self, const Expression& e) -> bool {
			if (e.kind == ExpressionKind::Assign) {
				return true;
			}
			// A helper writes the caller's storage only through a reference or a pointer parameter.
			if (e.kind == ExpressionKind::Call && e.left->kind == ExpressionKind::Identifier) {
				const auto takesAddress = [](const HelperFunction& helper) {
					return std::any_of(helper.definition->parameters.begin(), helper.definition->parameters.end(),
						[](const Parameter& p) { return p.isMutableReference() || p.type.isPointer; });
				};
				const auto overloads = _overloads.find(e.left->name);
				const auto single = _helpers.find(e.left->name);
				if (overloads != _overloads.end()
					? std::any_of(overloads->second.begin(), overloads->second.end(),
						[&](const HelperFunction* h) { return takesAddress(*h); })
					: single != _helpers.end() && takesAddress(single->second)) {
					return true;
				}
			}
			for (const Expression* child: { e.left.get(), e.right.get() }) {
				if (child && self(self, *child)) return true;
			}
			return std::any_of(e.arguments.begin(), e.arguments.end(),
				[&](const ExpressionPtr& a) { return self(self, *a); });
		};
		std::set<std::string> roots;
		for (const Passed& p: passed) {
			if (p.temporary) roots.insert(p.root);
		}
		for (size_t i = 0; i < call.arguments.size(); ++i) {
			const Parameter& parameter = definition.parameters[first + i];
			if (parameter.isMutableReference()) {
				continue;
			}
			std::set<std::string> names;
			collectIdentifiers(*call.arguments[i], names, false);
			const bool sharesRoot = std::any_of(names.begin(), names.end(),
				[&](const std::string& n) { return roots.count(n) > 0; });
			if ((parameter.type.isPointer && sharesRoot) || mayWrite(mayWrite, *call.arguments[i])) {
				throw CompileError("the call to \"" + definition.name + "\" passes an element or member through "
					"a reference parameter, and another argument reads or writes the same storage "
					"(a pointer to it, or an expression with side effects); the reference is a copy "
					"that is written back after the call, so that is not lowered yet");
			}
		}
	}

	// Two places of one variable that no run can make the same (different members, or different
	// literal indices at the first step they differ) are separate copies in and out, so the callee
	// cannot tell them from two variables. One that is a prefix of the other, or a dynamic index,
	// may overlap and stays refused.
	const auto disjoint = [](const std::vector<AccessStep>& a, const std::vector<AccessStep>& b) {
		for (size_t k = 0; k < a.size() && k < b.size(); ++k) {
			if (a[k].isDereference || a[k].isArrow || b[k].isDereference || b[k].isArrow
				|| a[k].isIndex != b[k].isIndex) {
				return false;
			}
			if (!a[k].isIndex) {
				if (a[k].memberName != b[k].memberName) {
					return true;
				}
				continue;
			}
			const bool literals = a[k].index->kind == ExpressionKind::IntLiteral
				&& b[k].index->kind == ExpressionKind::IntLiteral;
			if (!literals) {
				return false;
			}
			if (a[k].index->intValue != b[k].index->intValue) {
				return true;
			}
		}
		return false;
	};

	for (size_t i = 0; i < passed.size(); ++i) {
		for (size_t j = i + 1; j < passed.size(); ++j) {
			const bool sameRoot = passed[i].root == passed[j].root
				&& passed[i].rootExpr->fileScope == passed[j].rootExpr->fileScope;
			const Binding* bi = findBinding(*passed[i].rootExpr);
			const Binding* bj = findBinding(*passed[j].rootExpr);
			const bool viaAddress = (passed[i].temporary || passed[j].temporary)
				&& ((bi && bi->isAddress) || (bj && bj->isAddress));
			const bool apart = sameRoot && !viaAddress && disjoint(passed[i].steps, passed[j].steps);
			if (!apart && ((sameRoot && (passed[i].temporary || passed[j].temporary)) || viaAddress)) {
				throw CompileError("the call to \"" + definition.name + "\" passes the same variable "
					"through two reference parameters, one of them as a member or element, and the two "
					"places may overlap (one holds the other, or an index is not a literal); that is "
					"not lowered yet");
			}
		}
	}
}

} // namespace mslc
