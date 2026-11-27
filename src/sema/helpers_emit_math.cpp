#include "sema/emitter.h"

namespace mslc {

// A parameter is a value the body may assign to, as in C++, and the caller's
// copy must not change, so each named one is copied into a variable of the
// helper's own and the body reads and writes that.
void Emitter::emitHelper(const HelperFunction& helper) {
	const FunctionDecl& definition = *helper.definition;

	_helper = &definition;
	_bindings.clear();
	bindFileScopeArrays(definition.globalsBefore);
	_terminated = false;
	_controlDepth = 0;

	_builder.setSection(spirv::Section::Functions);
	const Id function = helper.ids.at(helperScope(helper));
	_builder.emitDeclTypedAt(spirv::OpFunction, helper.returnType, function,
		{ kFunctionControlNone, helper.type });

	std::vector<Id> values;
	for (size_t i = 0; i < helper.parameterTypes.size(); ++i) {
		if (helper.parameterTypes[i] == InvalidId) {
			values.push_back(helper.threadgroupVariables[i]);
			continue;
		}
		values.push_back(_builder.emitTyped(spirv::OpFunctionParameter, helper.parameterTypes[i], { }));

		// SPV_KHR_physical_storage_buffer wants exactly one of Aliased or Restrict on a
		// pointer parameter. Metal pointers may alias (f(out, out) is legal), so a
		// driver must not assume otherwise.
		const Parameter& parameter = definition.parameters[i];
		if (parameter.type.isPointer
			&& (parameter.type.addressSpace == AddressSpace::Device
				|| parameter.type.addressSpace == AddressSpace::Constant)) {
			_builder.emit(spirv::OpDecorate, { values.back(), static_cast<uint32_t>(spirv::Decoration::Aliased) });
		}
	}

	// A declaration is a function with no blocks, resolved when the module is linked.
	if (!definition.body) {
		_builder.emit(spirv::OpFunctionEnd, { });
		linkageAttributes(function, linkageName(definition), kLinkageImport);
		_helper = nullptr;
		return;
	}

	beginBlock(_builder.nextId());
	for (size_t i = 0; i < values.size(); ++i) {
		const Parameter& parameter = definition.parameters[i];
		if (parameter.name.empty()) {
			continue;
		}

		if (parameter.type.resource != ResourceKind::None) {
			Binding bound;
			bound.id = values[i];
			bound.isPointer = true;
			bound.pointeeType = resourcePointee(parameter.type);
			bound.storageClass = spirv::StorageClass::UniformConstant;
			bound.pointeeMsl = parameter.type;
			_bindings[parameter.name] = bound;
			continue;
		}

		if (parameter.type.isPointer) {
			if (parameter.type.addressSpace == AddressSpace::Device
				|| parameter.type.addressSpace == AddressSpace::Constant) {
				// A whole buffer, passed as the pointer value the argument gave:
				// p[i], *p and p->field walk it like an entry point's buffer.
				const auto [pointee, block] = bufferParamTypes(parameter.type);
				Binding bound;
				bound.id = values[i];
				bound.isPointer = true;
				bound.pointeeType = pointee;
				bound.bufferPointeeType = block;
				bound.storageClass = spirv::StorageClass::PhysicalStorageBuffer;
				bound.structType = parameter.type.namedType.empty()
					? nullptr : _unit.findStruct(parameter.type.namedType);
				bound.isBuffer = true;
				bound.ownAddress = true;
				bound.readOnly = parameter.type.isConst
					|| parameter.type.addressSpace == AddressSpace::Constant;
				bound.pointeeMsl = parameter.type;
				bound.pointeeMsl.isPointer = false;
				bound.pointeeMsl.isConst = false;
				_bindings[parameter.name] = bound;
				continue;
			}
			const Type array = boundArray(parameter.type, helper.arrayLengths[i]);
			bindLocal(parameter.name, values[i], arrayTypeOf(array), array,
				parameter.type.addressSpace == AddressSpace::Threadgroup
					? spirv::StorageClass::Workgroup : spirv::StorageClass::Function);
			continue;
		}

		if (parameter.isMutableReference()) {
			bindLocal(parameter.name, values[i], declaredTypeOf(parameter.type), parameter.type);
			continue;
		}

		const Id type = helper.parameterTypes[i];
		const Id variable = _builder.emitDeclTyped(spirv::OpVariable,
			_types.pointer(spirv::StorageClass::Function, type),
			{ static_cast<uint32_t>(spirv::StorageClass::Function) });
		_builder.emit(spirv::OpStore, { variable, values[i] });
		bindLocal(parameter.name, variable, type, parameter.type);
	}

	emitFunctionBody(*definition.body);

	// A path that reaches the end of a function returning a value is undefined
	// in C++ and Apple only warns for it, so it is not made up a value. A block
	// left open by a structured if whose branches all return is dead, and this
	// terminates that too.
	if (!_terminated) {
		terminate(returnsVoid(definition) ? spirv::OpReturn : spirv::OpUnreachable, { });
	}
	_builder.emit(spirv::OpFunctionEnd, { });

	_helper = nullptr;
}

// The operand of a return in a function returning void: an expression of type void, such as a call to another
// function returning void, is allowed and evaluated ([stmt.return]); any other value is an error.
// A comma's left operand is a statement of its own, and each value of a conditional is a return operand too.
void Emitter::emitVoidReturnValue(const Expression& expression, const std::string& quoted) {
	if (isVoidCast(expression)) {
		emitExpressionStatement(expression);
		return;
	}
	if (expression.kind == ExpressionKind::Comma) {
		emitExpressionStatement(*expression.left);
		emitVoidReturnValue(*expression.right, quoted);
		return;
	}
	if (expression.kind == ExpressionKind::Conditional) {
		emitVoidConditional(expression, &quoted);
		return;
	}
	if (expression.kind == ExpressionKind::Call && emitVoidCall(expression, &quoted)) {
		return;
	}
	throw CompileError(quoted + " returns void, so its return cannot carry a value");
}

// A call that has no value, or whose value is dropped: a helper, or a builtin returning void (an atomic store,
// a barrier, a texture write). Returns false for any other call, which is then an expression. With
// returnedBy, the call is the operand of a return in that function, so a helper has to return void.
bool Emitter::emitVoidCall(const Expression& call, const std::string* returnedBy) {
	const Expression* receiver = nullptr;
	std::vector<Id> prepared;
	if (HelperFunction* helper = findHelper(call, receiver, prepared)) {
		if (returnedBy && !returnsVoid(*helper->definition)) {
			throw CompileError(*returnedBy + " returns void, so its return cannot carry a value");
		}
		emitHelperCall(call, *helper, receiver, prepared);
		return true;
	}
	const std::string name = call.left->kind == ExpressionKind::Identifier
		? builtinCalleeName(call.left->name) : std::string();
	if (name == "atomic_store_explicit") {
		emitAtomicCall(call);
		return true;
	}
	if (name == "threadgroup_barrier" || name == "simdgroup_barrier") {
		emitBarrierCall(call);
		return true;
	}
	if (call.left->kind == ExpressionKind::Member && call.left->memberName == "write") {
		emitTextureWrite(call, textureReceiver(call));
		return true;
	}
	if (call.left->kind == ExpressionKind::Member && call.left->memberName == "fence") {
		emitTextureFence(call, textureReceiver(call));
		return true;
	}
	return false;
}

// c ? a : b with no value, as the if it stands for. With returnedBy, each value is a return operand.
void Emitter::emitVoidConditional(const Expression& expression, const std::string* returnedBy) {
	const Id condition = emitCondition(*expression.left);
	const Id thenLabel = _builder.nextId();
	const Id elseLabel = _builder.nextId();
	const Id mergeLabel = _builder.nextId();
	_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
	terminate(spirv::OpBranchConditional, { condition, thenLabel, elseLabel });

	const ControlDepthScope depth(_controlDepth);
	for (size_t i = 0; i < 2; ++i) {
		beginBlock(i == 0 ? thenLabel : elseLabel);
		if (returnedBy) {
			emitVoidReturnValue(*expression.arguments.at(i), *returnedBy);
		} else {
			emitExpressionStatement(*expression.arguments.at(i));
		}
		branchUnlessTerminated(mergeLabel);
	}
	beginBlock(mergeLabel);
}

void Emitter::emitHelperReturn(const Statement& statement) {
	const std::string quoted = "\"" + _helper->name + "\"";

	if (returnsVoid(*_helper)) {
		if (statement.expression) {
			emitVoidReturnValue(*statement.expression, quoted);
		}

		terminate(spirv::OpReturn, { });
		return;
	}

	if (!statement.expression) {
		throw CompileError(quoted + " returns " + typeName(_helper->returnType)
			+ ", so a return needs a value");
	}

	if (!returnsVoid(*_helper)) {
		requireEnumValue(_helper->returnType.enumName, *statement.expression, "the value " + quoted + " returns");
	}
	const bool braced = statement.expression->kind == ExpressionKind::InitList && !statement.expression->constructType;
	if (braced && !_helper->returnType.enumName.empty()) {
		throw CompileError("a braced list is returned as " + typeName(_helper->returnType) + ", an enum, which is not lowered");
	}
	const Id value = braced ? emitInitListValue(_helper->returnType, *statement.expression)
		: emitExpression(*statement.expression);
	terminate(spirv::OpReturnValue, { convertImplicit(value, declaredTypeOf(_helper->returnType)) });
}

Id Emitter::emitMathBuiltin(const MathBuiltin& builtin,
	const std::vector<ExpressionPtr>& arguments) {

	const std::string name = builtin.name;
	if (arguments.size() != builtin.arity) {
		throw CompileError(name + " takes " + std::to_string(builtin.arity) + " argument"
			+ (builtin.arity == 1 ? "" : "s") + ", and this call passes "
			+ std::to_string(arguments.size()));
	}

	const bool isDerivative = builtin.shape == MathShape::Fwidth || builtin.shape == MathShape::Dfdx
		|| builtin.shape == MathShape::Dfdy;
	if (isDerivative && _entryPoint->stage != Stage::Fragment) {
		throw StageError(name + " is only available in fragment functions");
	}

	std::vector<Id> values;
	for (const ExpressionPtr& argument: arguments) {
		const Id value = emitNonComponent(*argument, name);
		const Id type = _builder.typeOf(value);
		if (_types.matrixInfo(type)) {
			throw CompileError(name + " takes scalars or vectors, not a matrix");
		}
		// A bool is one bit wide and a struct or pointer has no width at all.
		if (!_types.isFloat(type) && _types.bitWidth(type) < 8) {
			throw CompileError(name + " takes numeric scalars or vectors");
		}
		values.push_back(value);
	}

	if (isDerivative) {
		const spirv::Opcode opcode = builtin.shape == MathShape::Dfdx ? spirv::OpDPdx
			: builtin.shape == MathShape::Dfdy ? spirv::OpDPdy : spirv::OpFwidth;
		const Id type = _builder.typeOf(values[0]);
		if (!_types.isFloat(type)) {
			throw CompileError(name + " takes float or half scalars or vectors");
		}

		if (_types.bitWidth(type) == 16) {
			const Id float32Type = _types.vector(ScalarKind::Float, _types.vectorWidth(type));
			const Id widened = convert(values[0], type, float32Type);
			if (builtin.shape != MathShape::Fwidth) {
				const Id result = _builder.emitTyped(opcode, float32Type, { widened });
				return convert(result, float32Type, type);
			}
			// Apple narrows |dx| and |dy| to half before adding them (measured on the GPU:
			// 1024 - 2^-10 plus 1.5 is 1026, not the 1025 a float32 sum gives). Core
			// SPIR-V derivatives are 32-bit, so each is taken in float32 and narrowed.
			if (_glslSet == InvalidId) {
				std::vector<uint32_t> setName;
				spirv::Builder::appendString(setName, "GLSL.std.450");
				_glslSet = _builder.emitDecl(spirv::OpExtInstImport, setName);
			}
			constexpr uint32_t kFAbs = 4;
			const auto magnitude = [&](spirv::Opcode derivative) {
				const Id difference = convert(_builder.emitTyped(derivative, float32Type, { widened }), float32Type, type);
				return _builder.emitTyped(spirv::OpExtInst, type, { _glslSet, kFAbs, difference });
			};
			const Id horizontal = magnitude(spirv::OpDPdx);
			const Id vertical = magnitude(spirv::OpDPdy);
			return _builder.emitTyped(spirv::OpFAdd, type, { horizontal, vertical });
		}

		return _builder.emitTyped(opcode, type, { values[0] });
	}

	// refract's eta is a scalar of its own, so only the first two of its
	// arguments say what type the call is in.
	const size_t sharing = builtin.shape == MathShape::Refract ? 2 : values.size();

	const bool takesIntegers = builtin.signedInstruction != kNoInstruction;

	// The type the call is in: the first vector argument's, since a scalar
	// beside it is broadcast. Among scalars alone it is the first float
	// argument's, as Apple types pow(x, 2) as float, or else the first's.
	Id type = _builder.typeOf(values[0]);
	for (size_t i = 0; i < sharing; ++i) {
		const Id argumentType = _builder.typeOf(values[i]);
		if (_types.vectorWidth(argumentType) > 1) {
			type = argumentType;
			break;
		}
		if (!_types.isFloat(type) && _types.isFloat(argumentType)) {
			type = argumentType;
		}
	}

	const uint32_t width = _types.vectorWidth(type);
	const Id component = width > 1 ? _types.componentOf(type) : type;
	const bool isFloat = _types.isFloat(type);
	const bool isSigned = _types.isSignedInt(type);
	const uint32_t bits = _types.bitWidth(type);

	const bool takesScalars = builtin.shape == MathShape::Componentwise
		|| builtin.shape == MathShape::Saturate || builtin.shape == MathShape::RoundHalfAway
		|| builtin.shape == MathShape::SignExact || builtin.shape == MathShape::FloatRemainder;
	// Metal has no double, and GLSL.std.450's transcendentals take only 16 and
	// 32 bits.
	if (isFloat ? bits > 32 : !takesIntegers) {
		throw CompileError(name + " takes float or half" + (takesIntegers ? " or integer" : "")
			+ (takesScalars ? " scalars or vectors" : " vectors"));
	}

	if (!takesScalars && width == 1) {
		throw CompileError(name + " takes vectors, and Metal has no scalar " + name);
	}

	if (builtin.shape == MathShape::Cross && width != 3) {
		throw CompileError("cross takes three-component vectors, and these have "
			+ std::to_string(width));
	}

	// A scalar beside a vector is broadcast, as Metal converts a scalar to any
	// vector. Beside a float scalar only an integer is converted, and only
	// for a builtin with no integer form: Apple reports min(int, float) and
	// mix(half, half, float) as ambiguous.
	for (size_t i = 0; i < sharing; ++i) {
		const Id argumentType = _builder.typeOf(values[i]);
		if (argumentType == type) {
			continue;
		}

		const bool converts = width == 1 && !takesIntegers && !_types.isFloat(argumentType);
		if (_types.vectorWidth(argumentType) != 1 || (width == 1 && !converts)) {
			throw CompileError("the arguments of " + name + " have to be one type, or a "
				"scalar beside a vector");
		}

		values[i] = width > 1 ? broadcast(values[i], type) : convert(values[i], argumentType, type);
	}

	if (builtin.shape == MathShape::Refract) {
		if (_types.vectorWidth(_builder.typeOf(values[2])) != 1) {
			throw CompileError("refract's third argument is the scalar ratio of the indices "
				"of refraction, not a vector");
		}
		values[2] = convert(values[2], _builder.typeOf(values[2]), component);
	}

	if (builtin.shape == MathShape::Dot) {
		return _builder.emitTyped(spirv::OpDot, component, { values[0], values[1] });
	}

	if (builtin.shape == MathShape::Saturate) {
		Id zero = _builder.emitDeclTyped(spirv::OpConstant, component, { 0u });
		// 1.0 as a half is 0x3C00, not the float's bits; 0.0 is zero bits in both.
		Id one = _builder.emitDeclTyped(spirv::OpConstant, component,
			{ bits == 16 ? 0x3C00u : 0x3F800000u });
		if (width > 1) {
			zero = _builder.emitDeclTyped(spirv::OpConstantComposite, type,
				std::vector<uint32_t>(width, zero));
			one = _builder.emitDeclTyped(spirv::OpConstantComposite, type,
				std::vector<uint32_t>(width, one));
		}
		values.push_back(zero);
		values.push_back(one);
	}

	if (builtin.shape == MathShape::RoundHalfAway) {
		return emitRoundHalfAway(values[0], type, bits);
	}
	if (builtin.shape == MathShape::SignExact) {
		return emitSignExact(values[0], type, bits);
	}
	if (builtin.shape == MathShape::FloatRemainder) {
		if (bits == 16) {
			// The quotient of a half remainder can pass the half range (65504 / 6), and a
			// driver that forms it in half precision answers infinity where Apple answers
			// the remainder, so it is computed in float and narrowed.
			const uint32_t width = _types.vectorWidth(type);
			const Id wide = width > 1 ? _types.vector(ScalarKind::Float, width) : _types.scalar(ScalarKind::Float);
			const Id a = convert(values[0], type, wide);
			const Id b = convert(values[1], type, wide);
			return convert(_builder.emitTyped(spirv::OpFRem, wide, { a, b }), wide, type);
		}
		return _builder.emitTyped(spirv::OpFRem, type, { values[0], values[1] });
	}

	const uint32_t instruction = isFloat ? builtin.floatInstruction
		: isSigned ? builtin.signedInstruction : builtin.unsignedInstruction;
	if (instruction == kNoInstruction) {
		throw CompileError(name + " of an unsigned integer is not lowered yet");
	}

	if (_glslSet == InvalidId) {
		// The set name is a literal string operand, not an OpString id.
		std::vector<uint32_t> setName;
		spirv::Builder::appendString(setName, "GLSL.std.450");
		_glslSet = _builder.emitDecl(spirv::OpExtInstImport, setName);
	}

	const Id result = builtin.shape == MathShape::VectorToScalar ? component : type;
	std::vector<uint32_t> operands { _glslSet, instruction };
	operands.insert(operands.end(), values.begin(), values.end());
	return _builder.emitTyped(spirv::OpExtInst, result, operands);
}

// A float constant of a scalar or vector float type: every component is the value.
Id Emitter::packFloatConstant(Id type, float value) {
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof bits);
	const uint32_t width = _types.vectorWidth(type);
	const Id component = _types.scalar(ScalarKind::Float);
	const Id scalar = _builder.emitDeclTyped(spirv::OpConstant, component, { bits });
	if (width <= 1) {
		return scalar;
	}
	return _builder.emitDeclTyped(spirv::OpConstantComposite, type, std::vector<uint32_t>(width, scalar));
}

// A constant vector of 32-bit integers, one word per component.
Id Emitter::packConstantVector(ScalarKind kind, const std::vector<uint32_t>& words) {
	const Id component = _types.scalar(kind);
	std::vector<uint32_t> parts;
	for (const uint32_t word: words) {
		parts.push_back(_builder.emitDeclTyped(spirv::OpConstant, component, { word }));
	}
	return _builder.emitDeclTyped(spirv::OpConstantComposite, _types.vector(kind, uint32_t(words.size())), parts);
}

Id Emitter::glslInstruction(uint32_t instruction, Id type, const std::vector<Id>& operands) {
	if (_glslSet == InvalidId) {
		std::vector<uint32_t> setName;
		spirv::Builder::appendString(setName, "GLSL.std.450");
		_glslSet = _builder.emitDecl(spirv::OpExtInstImport, setName);
	}
	std::vector<uint32_t> words { _glslSet, instruction };
	words.insert(words.end(), operands.begin(), operands.end());
	return _builder.emitTyped(spirv::OpExtInst, type, words);
}

// a / (2^period - 1), correctly rounded, for each of count fields of a magnitude 0 <= a <= 2^period - 1.
// The quotient's binary expansion is a's period bits repeated without end, so its float is made from
// the bits rather than by a division, which Vulkan lets be 2.5 ULP out: L, the zeros ahead of a's top
// bit, puts the leading 1 at 2^-(L + 1), after which come a's bits rotated left by L, repeated. The
// first 32 of them give the 24 bits of the significand and the bit below, which rounds it up when set;
// the bits further on are never all 0, so there is no tie. 0 and 1 are their own values.
Id Emitter::normalizedQuotient(Id magnitudes, uint32_t period, uint32_t count) {
	constexpr uint32_t kFindUMsb = 75;
	const Id uintVector = _types.vector(ScalarKind::UInt, count);
	const Id floatVector = _types.vector(ScalarKind::Float, count);
	const Id boolVector = _types.vector(ScalarKind::Bool, count);
	const auto splat = [&](uint32_t word) {
		return packConstantVector(ScalarKind::UInt, std::vector<uint32_t>(count, word));
	};
	const auto op = [&](spirv::Opcode opcode, Id a, Id b) { return _builder.emitTyped(opcode, uintVector, { a, b }); };
	const uint32_t top = (1u << period) - 1;
	const Id a = magnitudes;
	// FindUMsb of 0 is -1: L is then period, whose shifts below stay in range; the lane is replaced.
	const Id msb = _builder.emitTyped(spirv::OpBitcast, uintVector,
		{ glslInstruction(kFindUMsb, _types.vector(ScalarKind::Int, count), { a }) });
	const Id leading = op(spirv::OpISub, splat(period - 1), msb);
	const Id rotated = op(spirv::OpBitwiseAnd, op(spirv::OpBitwiseOr, op(spirv::OpShiftLeftLogical, a, leading),
		op(spirv::OpShiftRightLogical, a, op(spirv::OpISub, splat(period), leading))), splat(top));
	Id window = InvalidId;
	for (uint32_t k = 1;; ++k) {
		const int shift = 32 - int(k * period);
		const Id part = shift >= 0 ? op(spirv::OpShiftLeftLogical, rotated, splat(uint32_t(shift)))
			: op(spirv::OpShiftRightLogical, rotated, splat(uint32_t(-shift)));
		window = window == InvalidId ? part : op(spirv::OpBitwiseOr, window, part);
		if (shift <= 0) {
			break;
		}
	}
	const Id significand = op(spirv::OpIAdd, op(spirv::OpShiftRightLogical, window, splat(8)),
		op(spirv::OpBitwiseAnd, op(spirv::OpShiftRightLogical, window, splat(7)), splat(1)));
	// The exponent field of 2^-(L + 1) is 126 - L; a significand that rounded up to 2^24 carries into it.
	const Id bits = op(spirv::OpISub, op(spirv::OpIAdd, op(spirv::OpShiftLeftLogical,
		op(spirv::OpISub, splat(126), leading), splat(23)), significand), splat(0x800000));
	const Id quotient = _builder.emitTyped(spirv::OpBitcast, floatVector, { bits });
	const Id isZero = _builder.emitTyped(spirv::OpIEqual, boolVector, { a, splat(0) });
	const Id isOne = _builder.emitTyped(spirv::OpUGreaterThanEqual, boolVector, { a, splat(top) });
	return _builder.emitTyped(spirv::OpSelect, floatVector, { isZero, packFloatConstant(floatVector, 0.0f),
		_builder.emitTyped(spirv::OpSelect, floatVector, { isOne, packFloatConstant(floatVector, 1.0f), quotient }) });
}

// The <metal_pack> functions, written out as the conversions the MSL specification gives in 8.7.1
// (normalized integers) and 8.7.7 (sRGB), rather than with GLSL.std.450's Pack and Unpack
// instructions, whose rounding and precision Vulkan leaves to the implementation. Component i is
// held in bits [i * bits, (i + 1) * bits) of the uint, the first component in the lowest bits.
// The work is done in float; a half result is converted from it, a half argument converted to it.
// c / max is the correctly rounded quotient (see normalizedQuotient), as 8.7.1.2 says Metal 2's
// conversions round, and so exact at 0, -1 and 1 as Table 8.6 asks.
Id Emitter::emitPackBuiltin(const PackBuiltin& builtin, const std::vector<ExpressionPtr>& arguments) {
	const std::string name = builtin.name;
	if (arguments.size() != 1) {
		throw CompileError(name + " takes 1 argument, and this call passes " + std::to_string(arguments.size()));
	}
	constexpr uint32_t kRoundEven = 2;
	constexpr uint32_t kPow = 26;
	constexpr uint32_t kNClamp = 81;
	constexpr uint32_t kUMin = 38;
	const uint32_t n = builtin.count;
	const Id uintType = _types.scalar(ScalarKind::UInt);
	const Id uintVector = _types.vector(ScalarKind::UInt, n);
	const Id intVector = _types.vector(ScalarKind::Int, n);
	const Id floatVector = _types.vector(ScalarKind::Float, n);
	const Id boolVector = _types.vector(ScalarKind::Bool, n);
	const Id floatTypeOut = _types.vector(builtin.floatKind, n);
	std::vector<uint32_t> shiftWords;
	for (uint32_t i = 0; i < n; ++i) {
		shiftWords.push_back(i * builtin.bits);
	}
	const Id shifts = packConstantVector(ScalarKind::UInt, shiftWords);
	const uint32_t unormMax = (1u << builtin.bits) - 1;
	const uint32_t snormMax = (1u << (builtin.bits - 1)) - 1;
	const std::string vectorName = std::string(scalarKindName(builtin.floatKind)) + std::to_string(n);

	// sRGB keeps the alpha channel linear: the colour components come from one vector and the
	// fourth from the other.
	const auto colourAndAlpha = [&](Id colour, Id alpha, Id type) {
		return _builder.emitTyped(spirv::OpVectorShuffle, type, { colour, alpha, 0, 1, 2, 7 });
	};

	if (!builtin.pack) {
		// The argument is a uint, any other scalar converted to one as a call converts it (MSL 2.24).
		const Id value = emitNonComponent(*arguments[0], name);
		const Id valueType = _builder.typeOf(value);
		if (!_types.scalarKindOf(valueType) || _types.bitWidth(valueType) == 0) {
			throw CompileError(name + " takes a uint");
		}
		const Id word = convert(value, valueType, uintType);
		const Id words = _builder.emitTyped(spirv::OpCompositeConstruct, uintVector, std::vector<Id>(n, word));
		Id result;
		if (builtin.encoding == PackEncoding::Snorm) {
			// Each field moved to the top of a word and back down with its sign: max(-1, c / max).
			std::vector<uint32_t> up;
			for (uint32_t i = 0; i < n; ++i) {
				up.push_back(32 - builtin.bits - i * builtin.bits);
			}
			const Id raised = _builder.emitTyped(spirv::OpShiftLeftLogical, uintVector,
				{ words, packConstantVector(ScalarKind::UInt, up) });
			const Id signedFields = _builder.emitTyped(spirv::OpShiftRightArithmetic, intVector,
				{ _builder.emitTyped(spirv::OpBitcast, intVector, { raised }),
					packConstantVector(ScalarKind::UInt, std::vector<uint32_t>(n, 32 - builtin.bits)) });
			// max(-1, c / max): the magnitude, at most max, over max, with the field's sign.
			constexpr uint32_t kSAbs = 5;
			const Id magnitudes = glslInstruction(kUMin, uintVector,
				{ _builder.emitTyped(spirv::OpBitcast, uintVector, { glslInstruction(kSAbs, intVector, { signedFields }) }),
					packConstantVector(ScalarKind::UInt, std::vector<uint32_t>(n, snormMax)) });
			const Id quotient = normalizedQuotient(magnitudes, builtin.bits - 1, n);
			const Id negative = _builder.emitTyped(spirv::OpSLessThan, boolVector,
				{ signedFields, packConstantVector(ScalarKind::Int, std::vector<uint32_t>(n, 0)) });
			result = _builder.emitTyped(spirv::OpSelect, floatVector,
				{ negative, _builder.emitTyped(spirv::OpFNegate, floatVector, { quotient }), quotient });
		} else {
			// c / max for each field.
			const Id fields = _builder.emitTyped(spirv::OpBitwiseAnd, uintVector,
				{ _builder.emitTyped(spirv::OpShiftRightLogical, uintVector, { words, shifts }),
					packConstantVector(ScalarKind::UInt, std::vector<uint32_t>(n, unormMax)) });
			result = normalizedQuotient(fields, builtin.bits, n);
			if (builtin.encoding == PackEncoding::Srgb) {
				// c <= 0.04045 ? c / 12.92 : powr((c + 0.055) / 1.055, 2.4)
				const Id low = _builder.emitTyped(spirv::OpFDiv, floatVector,
					{ result, packFloatConstant(floatVector, 12.92f) });
				const Id shifted = _builder.emitTyped(spirv::OpFDiv, floatVector,
					{ _builder.emitTyped(spirv::OpFAdd, floatVector, { result, packFloatConstant(floatVector, 0.055f) }),
						packFloatConstant(floatVector, 1.055f) });
				const Id high = glslInstruction(kPow, floatVector, { shifted, packFloatConstant(floatVector, 2.4f) });
				const Id isLow = _builder.emitTyped(spirv::OpFOrdLessThanEqual, boolVector,
					{ result, packFloatConstant(floatVector, 0.04045f) });
				const Id linear = _builder.emitTyped(spirv::OpSelect, floatVector, { isLow, low, high });
				result = colourAndAlpha(linear, result, floatVector);
			}
		}
		return builtin.floatKind == ScalarKind::Float ? result : convert(result, floatVector, floatTypeOut);
	}

	// A scalar fills the vector, converted as a call converts it; a vector has to be of the type (MSL 2.24).
	Id value = emitNonComponent(*arguments[0], name);
	Id valueType = _builder.typeOf(value);
	if (_types.scalarKindOf(valueType) && _types.bitWidth(valueType) != 0) {
		value = broadcast(convert(value, valueType, _types.scalar(builtin.floatKind)), floatTypeOut);
		valueType = floatTypeOut;
	}
	if (valueType != floatTypeOut) {
		throw CompileError(name + " takes a " + vectorName);
	}
	const Id floats = builtin.floatKind == ScalarKind::Float ? value : convert(value, valueType, floatVector);
	const Id zero = packFloatConstant(floatVector, 0.0f);
	// A NaN converts to 0 (8.7.1.2 and 8.7.7), by a select rather than by NClamp's handling of it.
	const Id isNan = _builder.emitTyped(spirv::OpIsNan, boolVector, { floats });
	const Id cleaned = _builder.emitTyped(spirv::OpSelect, floatVector, { isNan, zero, floats });
	Id fields;
	if (builtin.encoding == PackEncoding::Snorm) {
		// min(max(f * max, -max), max), rounded to nearest even.
		const Id limit = packFloatConstant(floatVector, float(snormMax));
		const Id clamped = glslInstruction(kNClamp, floatVector,
			{ _builder.emitTyped(spirv::OpFMul, floatVector, { cleaned, limit }),
				packFloatConstant(floatVector, -float(snormMax)), limit });
		const Id rounded = glslInstruction(kRoundEven, floatVector, { clamped });
		fields = _builder.emitTyped(spirv::OpBitwiseAnd, uintVector,
			{ _builder.emitTyped(spirv::OpBitcast, uintVector,
					{ _builder.emitTyped(spirv::OpConvertFToS, intVector, { rounded }) }),
				packConstantVector(ScalarKind::UInt, std::vector<uint32_t>(n, unormMax)) });
	} else {
		// min(max(f * max, 0), max), rounded to nearest even.
		const Id limit = packFloatConstant(floatVector, float(unormMax));
		const Id clamped = glslInstruction(kNClamp, floatVector,
			{ _builder.emitTyped(spirv::OpFMul, floatVector, { cleaned, limit }), zero, limit });
		fields = _builder.emitTyped(spirv::OpConvertFToU, uintVector,
			{ glslInstruction(kRoundEven, floatVector, { clamped }) });
		if (builtin.encoding == PackEncoding::Srgb) {
			// The colour: c clamped to [0, 1] (a NaN to 0), then c < 0.0031308 ? 12.92 * c :
			// 1.055 * powr(c, 1 / 2.4) - 0.055, scaled to 255, plus 0.5, its fraction dropped.
			const Id one = packFloatConstant(floatVector, 1.0f);
			const Id c = glslInstruction(kNClamp, floatVector, { cleaned, zero, one });
			const Id low = _builder.emitTyped(spirv::OpFMul, floatVector, { c, packFloatConstant(floatVector, 12.92f) });
			const Id curve = glslInstruction(kPow, floatVector, { c, packFloatConstant(floatVector, 1.0f / 2.4f) });
			const Id high = _builder.emitTyped(spirv::OpFSub, floatVector,
				{ _builder.emitTyped(spirv::OpFMul, floatVector, { curve, packFloatConstant(floatVector, 1.055f) }),
					packFloatConstant(floatVector, 0.055f) });
			const Id isLow = _builder.emitTyped(spirv::OpFOrdLessThan, boolVector,
				{ c, packFloatConstant(floatVector, 0.0031308f) });
			const Id encoded = _builder.emitTyped(spirv::OpSelect, floatVector, { isLow, low, high });
			const Id scaled = _builder.emitTyped(spirv::OpFAdd, floatVector,
				{ _builder.emitTyped(spirv::OpFMul, floatVector, { encoded, packFloatConstant(floatVector, 255.0f) }),
					packFloatConstant(floatVector, 0.5f) });
			const Id colour = _builder.emitTyped(spirv::OpConvertFToU, uintVector, { scaled });
			fields = colourAndAlpha(colour, fields, uintVector);
		}
	}
	const Id placed = _builder.emitTyped(spirv::OpShiftLeftLogical, uintVector, { fields, shifts });
	Id word = _builder.emitTyped(spirv::OpCompositeExtract, uintType, { placed, 0 });
	for (uint32_t i = 1; i < n; ++i) {
		word = _builder.emitTyped(spirv::OpBitwiseOr, uintType,
			{ word, _builder.emitTyped(spirv::OpCompositeExtract, uintType, { placed, i }) });
	}
	return word;
}

// round(x): the whole number nearest x, a half going away from zero. Trunc
// gives the whole part; when the fraction is at least a half the result is one
// further along the sign of x. Infinity and NaN come out as themselves, since
// their fraction is NaN and so is never at least a half.
Id Emitter::emitRoundHalfAway(Id value, Id type, uint32_t bits) {
	if (_glslSet == InvalidId) {
		std::vector<uint32_t> setName;
		spirv::Builder::appendString(setName, "GLSL.std.450");
		_glslSet = _builder.emitDecl(spirv::OpExtInstImport, setName);
	}

	const auto extended = [&](uint32_t instruction, Id operand) {
		return _builder.emitTyped(spirv::OpExtInst, type, { _glslSet, instruction, operand });
	};
	constexpr uint32_t kTrunc = 3;
	constexpr uint32_t kFAbs = 4;
	constexpr uint32_t kFSign = 6;

	const uint32_t width = _types.vectorWidth(type);
	const Id component = width > 1 ? _types.componentOf(type) : type;
	Id half = _builder.emitDeclTyped(spirv::OpConstant, component, { bits == 16 ? 0x3800u : 0x3F000000u });
	if (width > 1) {
		half = _builder.emitDeclTyped(spirv::OpConstantComposite, type,
			std::vector<uint32_t>(width, half));
	}

	const Id whole = extended(kTrunc, value);
	const Id fraction = _builder.emitTyped(spirv::OpFSub, type, { value, whole });
	const Id magnitude = extended(kFAbs, fraction);
	const Id boolType = width > 1 ? _types.vector(ScalarKind::Bool, width) : _boolType;
	const Id atLeastHalf = _builder.emitTyped(spirv::OpFOrdGreaterThanEqual, boolType, { magnitude, half });
	const Id further = _builder.emitTyped(spirv::OpFAdd, type, { whole, extended(kFSign, value) });
	return _builder.emitTyped(spirv::OpSelect, type, { atLeastHalf, further, whole });
}

// sign(x): FSign, except that a zero keeps its own sign and a NaN gives 0.
Id Emitter::emitSignExact(Id value, Id type, uint32_t bits) {
	if (_glslSet == InvalidId) {
		std::vector<uint32_t> setName;
		spirv::Builder::appendString(setName, "GLSL.std.450");
		_glslSet = _builder.emitDecl(spirv::OpExtInstImport, setName);
	}

	const uint32_t width = _types.vectorWidth(type);
	const Id component = width > 1 ? _types.componentOf(type) : type;
	Id zero = _builder.emitDeclTyped(spirv::OpConstant, component, { 0u });
	if (width > 1) {
		zero = _builder.emitDeclTyped(spirv::OpConstantComposite, type,
			std::vector<uint32_t>(width, zero));
	}
	static_cast<void>(bits);

	constexpr uint32_t kFSign = 6;
	const Id boolType = width > 1 ? _types.vector(ScalarKind::Bool, width) : _boolType;
	const Id sign = _builder.emitTyped(spirv::OpExtInst, type, { _glslSet, kFSign, value });
	const Id isZero = _builder.emitTyped(spirv::OpFOrdEqual, boolType, { value, zero });
	const Id isNaN = _builder.emitTyped(spirv::OpIsNan, boolType, { value });
	const Id keptZero = _builder.emitTyped(spirv::OpSelect, type, { isZero, value, sign });
	return _builder.emitTyped(spirv::OpSelect, type, { isNaN, zero, keptZero });
}

} // namespace mslc
