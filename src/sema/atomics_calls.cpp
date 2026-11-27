#include "sema/emitter.h"

namespace mslc {

bool isAtomicFunction(const std::string& name) {
	static const char* const names[] = {
		"atomic_fetch_add_explicit", "atomic_fetch_sub_explicit", "atomic_fetch_min_explicit",
		"atomic_fetch_max_explicit", "atomic_fetch_and_explicit", "atomic_fetch_or_explicit",
		"atomic_fetch_xor_explicit", "atomic_exchange_explicit", "atomic_load_explicit",
		"atomic_store_explicit", "atomic_compare_exchange_weak_explicit",
		"atomic_compare_exchange_strong_explicit",
	};
	return std::find(std::begin(names), std::end(names), name) != std::end(names);
}

Id Emitter::atomicAddress(const Expression& argument, const std::string& function, bool& isSigned) {
	const Expression* pointer = &argument;
	const Expression* index = nullptr;
	bool addressOfLocal = false;
	if (argument.kind == ExpressionKind::Unary && argument.unaryOperator == UnaryOperator::AddressOf) {
		const Expression& element = *argument.left;
		if (element.kind == ExpressionKind::Identifier) {
			pointer = &element;
			addressOfLocal = true;
		} else if (element.kind == ExpressionKind::Index && !element.isDereference
			&& element.left->kind == ExpressionKind::Identifier && element.arguments.size() == 1) {
			pointer = element.left.get();
			index = element.arguments[0].get();
		} else {
			throw CompileError(function + " takes &local for a threadgroup atomic, or &counters[i] for an atomic buffer");
		}
	}

	if (pointer->kind != ExpressionKind::Identifier) {
		throw CompileError(function + " takes &local for a scalar threadgroup atomic, an atomic buffer "
			"pointer, or the address of one of its elements");
	}
	const Binding* found = findBinding(*pointer);
	if (!found || !found->pointeeMsl.isAtomic
		|| (!found->isBuffer && found->storageClass != spirv::StorageClass::Workgroup)) {
		throw CompileError(function + " takes a device atomic_uint or atomic_int pointer, or &local for a "
			"scalar threadgroup atomic, and \""
			+ pointer->name + "\" is not one");
	}

	const Binding& binding = *found;
	if (binding.storageClass == spirv::StorageClass::Workgroup && (!addressOfLocal || index)) {
		throw CompileError(function + " takes the address of a scalar threadgroup atomic, as &" + pointer->name);
	}
	if (binding.isBuffer && addressOfLocal) {
		throw CompileError(function + " takes the address of an element of an atomic buffer, as &counters[i]");
	}
	if (binding.pointeeMsl.addressSpace == AddressSpace::Constant) {
		throw CompileError(function + " takes a device atomic, and \"" + pointer->name
			+ "\" points into the constant address space");
	}
	if (binding.readOnly && function != "atomic_load_explicit") {
		throw CompileError(function + " changes the atomic, and \"" + pointer->name
			+ "\" points at a constant or const one; only atomic_load_explicit reads it");
	}
	isSigned = binding.pointeeMsl.scalar == ScalarKind::Int;
	if (binding.storageClass == spirv::StorageClass::Workgroup) {
		return binding.id;
	}
	const Id indexId = index ? stepIndex(*index, "buffer element") : constantU32(0);
	return _builder.emitTyped(spirv::OpAccessChain,
		_types.pointer(spirv::StorageClass::PhysicalStorageBuffer, binding.pointeeType),
		{ bufferBase(binding), constantU32(0), indexId });
}

// The function a call of metal::fast::name or metal::precise::name is: "name", when the library declares
// it in those scopes (MathBuiltin::inFastPrecise, and select), or else nothing. Black-box: a call of each
// in each scope, xcrun metal, macOS 26: dot, mix, step, smoothstep, sign, cross, reflect, refract and the
// derivatives are not members.
static std::string floatVariantFunction(const std::string& name) {
	for (const std::string_view scope : { std::string_view("metal::fast::"), std::string_view("metal::precise::") }) {
		if (name.compare(0, scope.size(), scope) == 0) {
			const std::string function = name.substr(scope.size());
			const MathBuiltin* builtin = findMathBuiltin(function);
			if (function == "select" || (builtin && builtin->inFastPrecise)) {
				return function;
			}
		}
	}
	return std::string();
}

std::string builtinCalleeName(const std::string& name) {
	constexpr std::string_view prefix = "metal::";
	if (name.compare(0, prefix.size(), prefix) == 0
		&& name.find("::", prefix.size()) == std::string::npos) {
		return name.substr(prefix.size());
	}
	// The library's fast and precise variants of a function lower as the function itself, on float
	// operands (see emitCall). Implementation limit: SPIR-V's extended instructions carry Vulkan's
	// precision, which neither variant's bound in the MSL specification is promised to match.
	if (std::string function = floatVariantFunction(name); !function.empty()) {
		return function;
	}
	return name;
}

Id Emitter::emitAtomicCall(const Expression& expression) {
	const std::string name = builtinCalleeName(expression.left->name);
	const bool isStore = name == "atomic_store_explicit";
	const bool isLoad = name == "atomic_load_explicit";
	if (name == "atomic_compare_exchange_strong_explicit") {
		throw CompileError(name + " is not a Metal function; Metal declares only "
			"atomic_compare_exchange_weak_explicit");
	}
	if (name == "atomic_compare_exchange_weak_explicit") {
		return emitAtomicCompareExchange(expression);
	}

	const size_t expected = isLoad ? 2 : 3;
	if (expression.arguments.size() != expected) {
		throw CompileError(name + " takes " + std::to_string(expected) + " arguments: the pointer, "
			+ (isLoad ? "" : "the value, ") + "and memory_order_relaxed");
	}
	const Expression& order = *expression.arguments.back();
	if (order.kind != ExpressionKind::Identifier
		|| (order.name != "memory_order_relaxed" && order.name != "metal::memory_order_relaxed")) {
		throw CompileError(name + " takes memory_order_relaxed, the only memory order Metal has");
	}

	bool isSigned = false;
	const Id address = atomicAddress(*expression.arguments[0], name, isSigned);
	const Id type = isSigned ? _intType : _uintType;
	const Id scope = constantU32(_types.storageClassOf(_builder.typeOf(address)) == spirv::StorageClass::Workgroup ? 2 : 1);
	const Id semantics = constantU32(0);  // Relaxed

	if (isLoad) {
		return _builder.emitTyped(spirv::OpAtomicLoad, type, { address, scope, semantics });
	}

	const Id value = convertImplicit(emitNonComponent(*expression.arguments[1], name), type);
	if (isStore) {
		_builder.emit(spirv::OpAtomicStore, { address, scope, semantics, value });
		return InvalidId;
	}

	uint16_t opcode = spirv::OpAtomicIAdd;
	if (name == "atomic_fetch_sub_explicit") opcode = spirv::OpAtomicISub;
	else if (name == "atomic_fetch_min_explicit") opcode = isSigned ? spirv::OpAtomicSMin : spirv::OpAtomicUMin;
	else if (name == "atomic_fetch_max_explicit") opcode = isSigned ? spirv::OpAtomicSMax : spirv::OpAtomicUMax;
	else if (name == "atomic_fetch_and_explicit") opcode = spirv::OpAtomicAnd;
	else if (name == "atomic_fetch_or_explicit") opcode = spirv::OpAtomicOr;
	else if (name == "atomic_fetch_xor_explicit") opcode = spirv::OpAtomicXor;
	else if (name == "atomic_exchange_explicit") opcode = spirv::OpAtomicExchange;
	return _builder.emitTyped(opcode, type, { address, scope, semantics, value });
}

// bool atomic_compare_exchange_weak_explicit(pointer, &expected, desired, relaxed, relaxed).
// OpAtomicCompareExchange returns what the atomic held; that equals expected exactly when
// the exchange happened, and expected takes it otherwise, so storing it back unconditionally
// is the same as Metal's write on failure.
Id Emitter::emitAtomicCompareExchange(const Expression& expression) {
	const std::string name = "atomic_compare_exchange_weak_explicit";
	if (expression.arguments.size() != 5) {
		throw CompileError(name + " takes 5 arguments: the pointer, &expected, the desired value, and "
			"memory_order_relaxed twice");
	}
	for (size_t i = 3; i < 5; ++i) {
		const Expression& order = *expression.arguments[i];
		if (order.kind != ExpressionKind::Identifier
			|| (order.name != "memory_order_relaxed" && order.name != "metal::memory_order_relaxed")) {
			throw CompileError(name + " takes memory_order_relaxed, the only memory order Metal has");
		}
	}

	bool isSigned = false;
	const Id address = atomicAddress(*expression.arguments[0], name, isSigned);
	const Id type = isSigned ? _intType : _uintType;

	const Expression& expectedArgument = *expression.arguments[1];
	const Expression* expected = expectedArgument.kind == ExpressionKind::Unary
		&& expectedArgument.unaryOperator == UnaryOperator::AddressOf
		&& expectedArgument.left->kind == ExpressionKind::Identifier ? expectedArgument.left.get() : nullptr;
	if (!expected) {
		throw CompileError(name + " takes &expected, the address of a local scalar variable, as its "
			"second argument");
	}
	const Binding* found = findBinding(*expected);
	if (!found || !found->isPointer || found->isBuffer
		|| found->isAddress || found->storageClass != spirv::StorageClass::Function
		|| found->bufferPointeeType != InvalidId) {
		throw CompileError(name + " takes &expected, the address of a local scalar variable, and \""
			+ expected->name + "\" is not one");
	}
	const Type& expectedType = found->pointeeMsl;
	if (found->readOnly) {
		throw CompileError(name + " writes the old value to \"" + expected->name + "\", which is const");
	}
	if (expectedType.isAtomic || expectedType.isPointer || !expectedType.isScalar() || expectedType.isMatrix()
		|| !expectedType.namedType.empty() || !expectedType.enumName.empty() || expectedType.arrayLength
		|| expectedType.scalar != (isSigned ? ScalarKind::Int : ScalarKind::UInt)) {
		throw CompileError(name + " takes &expected of the type the atomic holds, " + (isSigned ? "int" : "uint")
			+ ", and \"" + expected->name + "\" is not one");
	}

	const Id desired = convertImplicit(emitNonComponent(*expression.arguments[2], name), type);
	const Id comparator = emitExpression(*expected);
	const Id scope = constantU32(_types.storageClassOf(_builder.typeOf(address)) == spirv::StorageClass::Workgroup ? 2 : 1);
	const Id semantics = constantU32(0);  // Relaxed
	const Id old = _builder.emitTyped(spirv::OpAtomicCompareExchange, type,
		{ address, scope, semantics, semantics, desired, comparator });
	_builder.emit(spirv::OpStore, { found->id, old });
	return _builder.emitTyped(spirv::OpIEqual, _boolType, { old, comparator });
}

// The mem_flags a barrier names, as the SPIR-V memory semantics they select, and
// the widest scope among them. Metal's mem_flags is an enum class, so only the
// qualified spelling joined with | is a flag set.
void collectMemFlags(const Expression& flags, const std::string& function,
	uint32_t& semantics, bool& crossesWorkgroup) {
	if (flags.kind == ExpressionKind::Binary && flags.binaryOperator == BinaryOperator::BitOr) {
		collectMemFlags(*flags.left, function, semantics, crossesWorkgroup);
		collectMemFlags(*flags.right, function, semantics, crossesWorkgroup);
		return;
	}
	const std::string prefix = flags.name.rfind("metal::mem_flags::", 0) == 0
		? "metal::mem_flags::" : "mem_flags::";
	if (flags.kind != ExpressionKind::Identifier || flags.name.rfind(prefix, 0) != 0) {
		throw CompileError(function + " takes mem_flags::mem_none, mem_device, mem_threadgroup or "
			"mem_texture, joined with |");
	}
	const std::string flag = flags.name.substr(prefix.size());
	if (flag == "mem_none") {
		return;
	}
	if (flag == "mem_device") {
		semantics |= 64;  // UniformMemory, which includes buffers
		crossesWorkgroup = true;
	} else if (flag == "mem_threadgroup") {
		semantics |= 256;  // WorkgroupMemory
	} else if (flag == "mem_texture") {
		semantics |= 2048;  // ImageMemory
		crossesWorkgroup = true;
	} else {
		throw CompileError(function + " does not lower mem_flags::" + flag + "; mem_none, mem_device, "
			"mem_threadgroup and mem_texture are the flags mslc supports");
	}
}

void Emitter::emitBarrierCall(const Expression& expression) {
	const std::string name = builtinCalleeName(expression.left->name);
	if (expression.arguments.size() != 1) {
		throw CompileError(name + " takes one argument: the mem_flags to order");
	}
	if (_entryPoint->stage != Stage::Kernel) {
		throw StageError(name + " is only supported in a kernel function; Vulkan allows a "
			"control barrier only in compute shaders");
	}

	uint32_t semantics = 0;
	bool crossesWorkgroup = false;
	collectMemFlags(*expression.arguments.front(), name, semantics, crossesWorkgroup);
	if (semantics != 0) {
		semantics |= 8;  // AcquireRelease
	}

	const bool isSimd = name == "simdgroup_barrier";
	const uint32_t executionScope = isSimd ? 3 : 2;  // Subgroup : Workgroup
	const uint32_t memoryScope = isSimd ? 3 : (crossesWorkgroup ? 1 : 2);  // Device : Workgroup
	_builder.emit(spirv::OpControlBarrier, { constantU32(executionScope), constantU32(memoryScope),
		constantU32(semantics) });
}

// as_type<T>(x), which the parser reads as a call of kAsTypeCallee.
bool isAsTypeCall(const Expression& call) {
	return call.left && call.left->kind == ExpressionKind::Identifier && call.left->name == kAsTypeCallee;
}

Id Emitter::emitCall(const Expression& expression) {
	if (isAsTypeCall(expression)) {
		return emitAsType(expression);
	}
	const Expression* receiver = nullptr;
	std::vector<Id> prepared;
	HelperFunction* helper = findHelper(expression, receiver, prepared);
	if (!helper && expression.left->kind == ExpressionKind::Member) {
		return emitTextureCall(expression);
	}

	if (!helper && expression.left->kind != ExpressionKind::Identifier) {
		throw CompileError("only a direct function call is supported");
	}

	if (helper) {
		if (returnsVoid(*helper->definition)) {
			throw CompileError("a call to \"" + helper->definition->name + "\" returns void and has "
				"no value");
		}

		return emitHelperCall(expression, *helper, receiver, prepared);
	}

	const std::string name = builtinCalleeName(expression.left->name);
	if (name == "threadgroup_barrier" || name == "simdgroup_barrier") {
		throw CompileError(name + " returns void and has no value");
	}

	if (name == "__metal_is_function_constant_defined") {
		return emitIsFunctionConstantDefined(expression);
	}
	if (name == "get_num_samples") {
		return emitNumSamples(expression);
	}


	if (isAtomicFunction(name)) {
		if (name == "atomic_store_explicit") {
			throw CompileError("atomic_store_explicit returns void and has no value");
		}
		return emitAtomicCall(expression);
	}
	// The fast and precise scopes declare float overloads only: a scalar of another type converts to float
	// (Apple types metal::fast::clamp(5, 0, 3) as float), and a vector of another type matches nothing.
	const bool floatOnly = !floatVariantFunction(expression.left->name).empty();

	if (name == "select") {
		const Id selected = emitSelectCall(expression);
		if (!floatOnly) {
			return selected;
		}
		// Converting the chosen operand is converting both and then choosing.
		return floatVariantResult(selected, expression.left->name);
	}

	if (name == "all" || name == "any") {
		return emitAllAnyCall(expression, name);
	}

	if (const PackBuiltin* pack = findPackBuiltin(name)) {
		return emitPackBuiltin(*pack, expression.arguments);
	}
	const MathBuiltin* builtin = findMathBuiltin(name);
	if (!builtin) {
		throw CompileError("function \"" + expression.left->name + "\" is not a builtin mslc "
			"recognises, and it is not a function declared in this file");
	}
	return emitMathBuiltin(*builtin, expression.arguments, floatOnly ? &expression.left->name : nullptr);
}

Id Emitter::floatVariantResult(Id value, const std::string& function) {
	const Id type = _builder.typeOf(value);
	if (_types.isFloat(type) && _types.bitWidth(type) == 32) {
		return value;
	}
	if (_types.vectorWidth(type) > 1 || _types.matrixInfo(type) || (!_types.isFloat(type) && _types.bitWidth(type) < 8)) {
		throw CompileError("no overload of \"" + function + "\" takes this argument: the fast and precise "
			"variants take float scalars and vectors, and a vector of another type does not convert");
	}
	return convert(value, type, _types.scalar(ScalarKind::Float));
}

// select(a, b, c): b where c holds, else a. Apple's library declares it only for a
// bool or boolN condition, never an integer vector, and a scalar condition beside
// vector operands applies to every lane (a numeric scalar converts as in an if).
// The operands are one type, or a scalar beside a vector, which is broadcast;
// two scalars of different types are ambiguous there, and so are they here.
Id Emitter::emitSelectCall(const Expression& expression) {
	if (expression.arguments.size() != 3) {
		throw CompileError("select takes 3 arguments, and this call passes "
			+ std::to_string(expression.arguments.size()));
	}

	Id values[3];
	for (size_t i = 0; i < 3; ++i) {
		values[i] = emitNonComponent(*expression.arguments[i], "select");
	}

	for (size_t i = 0; i < 2; ++i) {
		const Id type = _builder.typeOf(values[i]);
		if (_types.matrixInfo(type) || _types.bitWidth(type) == 0) {
			throw CompileError("select takes bool, numeric or vector operands");
		}
	}

	Id aType = _builder.typeOf(values[0]);
	Id bType = _builder.typeOf(values[1]);
	const uint32_t aWidth = _types.vectorWidth(aType);
	const uint32_t bWidth = _types.vectorWidth(bType);
	const Id conditionType = _builder.typeOf(values[2]);
	const uint32_t conditionWidth = _types.vectorWidth(conditionType);

	if (aWidth == 1 && bWidth == 1) {
		if (aType != bType) {
			throw CompileError("the operands of select have to be one type: Apple reports a call "
				"with two scalars of different types as ambiguous");
		}
		if (conditionWidth > 1) {
			throw CompileError("select with a vector condition takes vector operands");
		}
	} else if (aWidth > 1 && bWidth > 1) {
		if (aType != bType) {
			throw CompileError("the operands of select have to be one type, or a scalar beside a vector");
		}
	} else if (aWidth == 1) {
		values[0] = broadcast(values[0], bType);
		aType = bType;
	} else {
		values[1] = broadcast(values[1], aType);
		bType = aType;
	}

	const uint32_t width = _types.vectorWidth(aType);
	Id condition;
	if (conditionWidth > 1) {
		if (_types.componentOf(conditionType) != _boolType || conditionWidth != width) {
			throw CompileError("the condition of select beside " + std::to_string(width)
				+ "-component operands has to be a bool or a bool" + std::to_string(width));
		}
		condition = values[2];
	} else {
		condition = asCondition(values[2]);
	}

	return _builder.emitTyped(spirv::OpSelect, aType, { condition, values[1], values[0] });
}

// all(x) and any(x) of <metal_relational>: whether every, or some, component of a bool or boolN holds. Apple's
// library declares them for those types only, so a numeric scalar converts to bool as in an if, and a numeric
// vector, which does not convert to a bool vector, finds no function.
Id Emitter::emitAllAnyCall(const Expression& expression, const std::string& name) {
	if (expression.arguments.size() != 1) {
		throw CompileError(name + " takes 1 argument, and this call passes "
			+ std::to_string(expression.arguments.size()));
	}
	const Id value = emitNonComponent(*expression.arguments[0], name);
	const Id type = _builder.typeOf(value);
	if (_types.matrixInfo(type) || _types.bitWidth(type) == 0) {
		throw CompileError(name + " takes a bool or a bool vector");
	}
	if (_types.vectorWidth(type) == 1) {
		return asCondition(value);
	}
	if (_types.componentOf(type) != _boolType) {
		throw CompileError(name + " takes a bool vector, and a numeric vector does not convert to one");
	}
	return _builder.emitTyped(name == "all" ? spirv::OpAll : spirv::OpAny, _boolType, { value });
}

// The helper a call names. A call through "." is a member function when the
// receiver is a struct value, and then receiver is the expression to be passed
// first.
Emitter::HelperFunction* Emitter::findHelper(const Expression& call, const Expression*& receiver,
	std::vector<Id>& prepared) {
	receiver = nullptr;
	if (call.left->kind == ExpressionKind::Member) {
		const Expression& member = *call.left;
		const std::string suffix = "::" + member.memberName;
		const auto isMember = [&](const auto& entry) {
			const std::string& name = entry.first;
			return name.size() > suffix.size()
				&& name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
		};
		const Binding* texture = findBinding(*member.left);
		// A name only a member template has is looked up through the receiver's struct, which structOf
		// reads without emitting anything, so a call such as a texture's own method is left alone.
		const bool templateNamed = _memberTemplateNames.count(member.memberName)
			&& std::none_of(_helpers.begin(), _helpers.end(), isMember)
			&& [&] {
				const StructDecl* owner = structOf(*member.left);
				return owner && !memberTemplatesOf(owner->name, member.memberName).empty();
			}();
		if ((texture && texture->pointeeMsl.isTexture())
			|| (std::none_of(_helpers.begin(), _helpers.end(), isMember) && !templateNamed)) {
			return nullptr;
		}

		// A local names its struct, so the receiver is evaluated as the first
		// argument is; any other receiver is evaluated to learn what it is.
		std::string ownerName;
		if (texture && !texture->pointeeMsl.namedType.empty()) {
			ownerName = texture->pointeeMsl.namedType;
		} else {
			_pendingObject = emitExpression(*member.left);
			if (const std::string* owner = _types.structNameOf(_builder.typeOf(_pendingObject), nullptr)) {
				ownerName = *owner;
			}
		}
		const auto found = ownerName.empty() ? _helpers.end() : _helpers.find(ownerName + suffix);
		if (const std::vector<const FunctionTemplate*> templates = memberTemplatesOf(ownerName, member.memberName);
			!templates.empty()) {
			if (found != _helpers.end()) {
				throw CompileError("\"" + ownerName + suffix + "\" is both a member function and a member function "
					"template, which Apple takes as one overload set and mslc does not lower yet");
			}
			if (std::any_of(templates.begin(), templates.end(), [](const FunctionTemplate* declaration) {
					return declaration->staticMember;
				})) {
				throw CompileError("the static member function template \"" + ownerName + suffix + "\" is called "
					"through an object, which is not lowered; call it as " + ownerName + suffix + "<...>(...)");
			}
			// Apple takes a member function on a device or constant object only when it is qualified so.
			if (const OverloadArgument object = classifyArgument(*member.left);
				object.kind == OverloadArgument::Kind::Variable && !object.inThread) {
				throw CompileError("the member function template \"" + ownerName + suffix + "\" is called on an object "
					"in device, constant or threadgroup memory, which Apple rejects (a member function is for a "
					"thread object); copy the object to a local first");
			}
			receiver = member.left.get();
			return resolveOverload(call, {}, templates, {}, prepared, receiver);
		}
		if (found != _helpers.end() && !member.templateArguments.empty()) {
			throw CompileError("\"" + found->first + "\" is not a template, and the call gives it explicit template "
				"arguments, which Apple rejects (another struct declares a member template of that name)");
		}
		if (found != _helpers.end() && member.templateKeyword) {
			throw CompileError("\"" + found->first + "\" follows the 'template' keyword and is not a template, "
				"which Apple rejects (another struct declares a member template of that name)");
		}
		if (found == _helpers.end()) {
			throw CompileError("the receiver of \"." + member.memberName + "\" is not a struct with a "
				"member function of that name");
		}
		if (found->second.definition == _helper) {
			throw CompileError("recursive call: \"" + found->first + "\" calls itself; SPIR-V for Vulkan "
				"forbids recursion (Apple's compiler accepts it)");
		}
		const std::vector<Parameter>& parameters = found->second.definition->parameters;
		if (parameters.empty() || parameters[0].name != "this") {
			throw CompileError("\"" + found->first + "\" is a static member function; call it as "
				+ found->first + "(...)");
		}
		if (call.arguments.size() + 1 != parameters.size()) {
			throw CompileError("call to \"" + found->first + "\" passes " + std::to_string(call.arguments.size())
				+ " arguments, and it takes " + std::to_string(parameters.size() - 1));
		}

		receiver = member.left.get();
		return &found->second;
	}

	if (call.left->kind != ExpressionKind::Identifier) {
		return nullptr;
	}

	std::set<std::string> adl;
	const std::vector<std::string> names = namesOfCall(call, &adl);
	std::vector<HelperFunction*> set;
	for (const std::string& name: names) {
		const std::vector<HelperFunction*> named = helpersNamed(name);
		set.insert(set.end(), named.begin(), named.end());
	}
	// A template is an overload of a helper of its name, so the arguments choose between them.
	const std::vector<const FunctionTemplate*> templates = visibleTemplates(names, adl);
	// Apple binds a call that does not depend on the template's parameters where the template is declared,
	// so a function declared below it, found only by the end-of-file lookup, must not take such a call.
	if (isInstance(_helper) && !adl.empty() && !dependsOnTemplate(call)) {
		const auto below = std::any_of(set.begin(), set.end(), [&](const HelperFunction* entry) {
			return entry->firstOrder > callerOrder() && adl.count(entry->definition->name);
		}) || std::any_of(templates.begin(), templates.end(), [&](const FunctionTemplate* declaration) {
			return declaration->order > callerOrder() && adl.count(declaration->name);
		});
		if (below) {
			throw CompileError("a call inside a template whose arguments do not depend on its parameters finds a "
				"function declared below the template through argument-dependent lookup; Apple binds such a call "
				"where the template is declared, which is not lowered yet");
		}
	}
	// validateHelpers checked the calls of a plain helper by name (arity, declared above, defined);
	// an instance's calls are checked by resolving them.
	if (!templates.empty() || call.left->hasTemplateArguments || set.size() > 1
		|| (isInstance(_helper) && !set.empty())) {
		return resolveOverload(call, set, templates, adl, prepared);
	}
	// One function found among several names (argument-dependent lookup found the only one) is
	// still checked as a chosen one; a name found alone was checked by validateHelpers.
	return set.empty() ? nullptr : names.size() > 1 ? definedHelper(set.front()) : set.front();
}

// A function a call selects has to have a body here, or be one the unit imports.
Emitter::HelperFunction* Emitter::definedHelper(HelperFunction* entry) {
	if (!entry->definition->body && !_options.linkageObject) {
		throw CompileError("\"" + entry->definition->name + "(" + parameterSignature(*entry->definition)
			+ ")\" is declared but never defined");
	}
	// The scan for imports does not know the functions argument-dependent lookup adds.
	if (!entry->definition->body && entry->ids.empty()) {
		throw CompileError("\"" + entry->definition->name + "\" is declared but defined elsewhere, and "
			"this call finds it by argument-dependent lookup, which importing does not follow yet");
	}
	return entry;
}

} // namespace mslc
