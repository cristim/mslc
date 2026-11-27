#include "sema/emitter.h"

namespace mslc {

// A file-scope "constant" is folded to one of the module's constants, with no binding of its own.
Id Emitter::declareGlobalConstant(const VariableDeclaration& declaration) {
	if (declaration.type.isAtomic) {
		throw CompileError("a file-scope atomic constant is not lowered");
	}
	if (declaration.type.isMatrix() && declaration.type.isConstexpr) {
		throw CompileError("constexpr variable \"" + declaration.name
			+ "\" must be initialized by a constant expression");
	}
	if (!declaration.type.isPointer && declaration.type.isScalar() && declaration.type.namedType.empty()) {
		const Expression* entry = declaration.initializer.get();
		while (entry->kind == ExpressionKind::InitList && !entry->constructType
			&& entry->elements.size() == 1 && entry->elements.front().fieldName.empty()) {
			entry = entry->elements.front().value.get();
		}
		if (entry->kind != ExpressionKind::InitList) {
			requireEnumValue(declaration.type.enumName, *entry,
				"the constant \"" + declaration.name + "\"");
		}
	}
	_foldDomainError = false;
	const FoldedConstant folded = foldInitializer(declaration.type, *declaration.initializer);
	if (declaration.type.isPointer) {
		throw CompileError("a file-scope pointer constant is not lowered yet");
	}
	if (_foldDomainError && declaration.type.isConstexpr) {
		throw CompileError("constexpr variable \"" + declaration.name + "\" must be initialized by a "
			"constant expression");
	}
	_foldDomainError = false;
	const Id id = emitConstant(folded);

	ConstantBinding binding;
	binding.id = id;
	binding.structType = _unit.findStruct(declaration.type.namedType);
	_constants[declaration.name] = binding;
	_folded[declaration.name] = folded;

	return id;
}

// A [[function_constant(n)]] is recorded here and declared where it is first read. Of the other constants,
// only "constant bool b = is_function_constant_defined(c);" may read one, as another name for whether c
// is defined: an initialiser computed from a value given at pipeline creation is not folded.
void Emitter::declareFunctionConstant(const VariableDeclaration& declaration) {
	for (const auto& [name, other]: _functionConstants) {
		if (*other.declaration->functionConstantIndex == *declaration.functionConstantIndex) {
			throw CompileError("the function constants \"" + name + "\" and \"" + declaration.name
				+ "\" have the same index " + std::to_string(*declaration.functionConstantIndex));
		}
	}
	_functionConstants[declaration.name].declaration = &declaration;
}

// "constant bool b = is_function_constant_defined(c);" makes b another name for whether c is defined. Any
// other initialiser that reads a function constant is a derived one (declareDerivedFunctionConstant).
bool Emitter::isFunctionConstantDefinedAlias(const VariableDeclaration& declaration) const {
	const Expression& initializer = *declaration.initializer;
	return initializer.kind == ExpressionKind::Call && initializer.left
		&& initializer.left->kind == ExpressionKind::Identifier
		&& builtinCalleeName(initializer.left->name) == "__metal_is_function_constant_defined"
		&& initializer.arguments.size() == 1 && initializer.arguments[0]->kind == ExpressionKind::Identifier
		&& _functionConstants.count(initializer.arguments[0]->name);
}

void Emitter::declareFunctionConstantAlias(const VariableDeclaration& declaration) {
	const Type& type = declaration.type;
	if (type.scalar != ScalarKind::Bool || type.vectorWidth > 1 || type.isPointer || type.arrayLength) {
		throw CompileError("the constant \"" + declaration.name + "\" holds is_function_constant_defined(...) "
			"and has to be a bool");
	}
	_functionConstantDefinedAliases[declaration.name] = declaration.initializer->arguments[0]->name;
}

// The function constant a name stands for, through "constant bool b = is_function_constant_defined(c);" too.
const std::string* Emitter::functionConstantNamed(const std::string& name) const {
	const auto constant = _functionConstants.find(name);
	if (constant != _functionConstants.end()) {
		return &constant->first;
	}
	const auto alias = _functionConstantDefinedAliases.find(name);
	return alias == _functionConstantDefinedAliases.end() ? nullptr : &alias->second;
}

// The value of a function constant, or with `defined` the bool that says whether it was given one, as a
// specialization constant that defaults to zero (false).
Id Emitter::functionConstantValue(const std::string& name, bool defined) {
	FunctionConstant& constant = _functionConstants.at(name);
	_functionConstantReads[_helper ? _helper : _entryPoint].insert(name);
	const uint32_t index = *constant.declaration->functionConstantIndex;
	if (constant.value == InvalidId) {
		const ScalarKind kind = constant.declaration->type.scalar;
		const Id type = _types.scalar(kind);
		if (kind == ScalarKind::Bool) {
			constant.value = _builder.emitDeclTyped(spirv::OpSpecConstantFalse, type, { });
		} else if (kind == ScalarKind::Long || kind == ScalarKind::ULong) {
			constant.value = _builder.emitDeclTyped(spirv::OpSpecConstant, type, { 0u, 0u });
		} else {
			constant.value = _builder.emitDeclTyped(spirv::OpSpecConstant, type, { 0u });
		}
		_builder.emit(spirv::OpDecorate, { constant.value, static_cast<uint32_t>(spirv::Decoration::SpecId),
			kFunctionConstantSpecializationID + index });
		constant.defined = _builder.emitDeclTyped(spirv::OpSpecConstantFalse, _types.scalar(ScalarKind::Bool), { });
		_builder.emit(spirv::OpDecorate, { constant.defined, static_cast<uint32_t>(spirv::Decoration::SpecId),
			kFunctionConstantDefinedSpecializationID + index });
	}
	return defined ? constant.defined : constant.value;
}

// The size in bits of a component of a scalar kind, or 0 for one as_type does not take: bool, whose
// storage Apple does not define, void and double.
static uint32_t asTypeComponentBits(ScalarKind kind) {
	switch (kind) {
		case ScalarKind::Char: case ScalarKind::UChar: return 8;
		case ScalarKind::Short: case ScalarKind::UShort: case ScalarKind::Half: return 16;
		case ScalarKind::Int: case ScalarKind::UInt: case ScalarKind::Float: return 32;
		case ScalarKind::Long: case ScalarKind::ULong: return 64;
		default: return 0;
	}
}

// as_type<T>(x): x's bits as a T, one OpBitcast. T and x are scalars or vectors of the same size in
// bytes, a three-component vector being the size of four (MSL specification 2.23). To a
// three-component vector from another vector, T's components are the first three of the bits seen
// as four, as the specification's float4 to float3 example has it. From a three-component vector
// only another three-component vector is taken, since the fourth component it is stored with has
// no value.
Id Emitter::emitAsType(const Expression& call) {
	const Expression& callee = *call.left;
	if (callee.templateArguments.size() != 1 || !callee.templateArguments[0].isType || call.arguments.size() != 1) {
		throw CompileError("as_type takes one type as its template argument and one operand");
	}
	const Type& target = callee.templateArguments[0].type;
	const std::string spelled = typeName(target);
	if (target.isPointer || target.arrayLength || !target.namedType.empty() || target.isMatrix()
		|| target.isPacked || target.isAtomic || !target.enumName.empty() || target.resource != ResourceKind::None
		|| target.addressSpace != AddressSpace::None || asTypeComponentBits(target.scalar) == 0) {
		throw CompileError("as_type<" + spelled + "> is not lowered: mslc reinterprets as a scalar or a vector "
			"of the integer, half and float types");
	}
	const Id value = emitNonComponent(*call.arguments[0], "as_type<" + spelled + ">");
	const Id valueType = _builder.typeOf(value);
	ScalarKind fromScalar;
	uint32_t fromWidth = 1;
	if (const auto scalar = _types.scalarKindOf(valueType)) {
		fromScalar = *scalar;
	} else if (const Id component = _types.componentOf(valueType); component != InvalidId
		&& !_types.matrixInfo(valueType)) {
		fromScalar = *_types.scalarKindOf(component);
		fromWidth = _types.vectorWidth(valueType);
	} else {
		throw CompileError("as_type<" + spelled + "> of this operand is not lowered: mslc reinterprets a "
			"scalar or a vector of the integer, half and float types");
	}
	const uint32_t fromBits = asTypeComponentBits(fromScalar);
	if (fromBits == 0) {
		throw CompileError("as_type<" + spelled + "> of a " + scalarKindName(fromScalar) + " value is not "
			"lowered: mslc reinterprets a scalar or a vector of the integer, half and float types");
	}
	const uint32_t toBits = asTypeComponentBits(target.scalar);
	const uint32_t toWidth = target.vectorWidth ? target.vectorWidth : 1;
	const auto stored = [](uint32_t width) { return width == 3 ? 4u : width; };
	if (fromBits * stored(fromWidth) != toBits * stored(toWidth)) {
		throw CompileError("as_type<" + spelled + "> needs an operand of the same size: it is "
			+ std::to_string(toBits * stored(toWidth) / 8) + " bytes, and the operand is "
			+ std::to_string(fromBits * stored(fromWidth) / 8));
	}
	if (fromWidth == 3 && toWidth != 3) {
		throw CompileError("as_type<" + spelled + "> of a three-component vector is lowered only to another "
			"three-component vector: the fourth component it is stored with has no value");
	}
	const Id toType = declaredTypeOf(target);
	if (toType == valueType) {
		return value;
	}
	if (toWidth == 3 && fromWidth != 3) {
		const Id four = _types.vector(target.scalar, 4);
		const Id bits = _builder.emitTyped(spirv::OpBitcast, four, { value });
		return _builder.emitTyped(spirv::OpVectorShuffle, toType, { bits, bits, 0, 1, 2 });
	}
	return _builder.emitTyped(spirv::OpBitcast, toType, { value });
}

Id Emitter::emitIsFunctionConstantDefined(const Expression& call) {
	if (call.arguments.size() != 1 || call.arguments[0]->kind != ExpressionKind::Identifier
		|| !_functionConstants.count(call.arguments[0]->name) || findResourceBinding(call.arguments[0]->name)) {
		throw CompileError("is_function_constant_defined takes the name of a function constant");
	}
	return functionConstantValue(call.arguments[0]->name, true);
}

// A constant computed from function constants, "constant bool b = n > 0;" with n one: its value is known only
// when the function is made, so it is a specialization constant of its own, an OpSpecConstantOp over theirs,
// which the driver works out from the values it is given. Only a bool or an integer of 32 bits can be one, and
// only the integer and logical operators: the operations an OpSpecConstantOp may take in a shader.
void Emitter::declareDerivedFunctionConstant(const VariableDeclaration& declaration) {
	const Type& type = declaration.type;
	if ((type.scalar != ScalarKind::Bool && type.scalar != ScalarKind::Int && type.scalar != ScalarKind::UInt)
		|| type.vectorWidth > 1 || type.isMatrix() || type.isPointer || type.arrayLength || !type.namedType.empty()
		|| !type.enumName.empty() || !declaration.initializer) {
		throw CompileError("the constant \"" + declaration.name + "\" is initialised from a function constant, and "
			"mslc lowers such a constant only as a bool, an int or a uint");
	}
	DerivedFunctionConstant& derived = _derivedFunctionConstants[declaration.name];
	derived.declaration = &declaration;
	std::set<std::string> read;
	collectIdentifiers(*declaration.initializer, read);
	for (const std::string& name: read) {
		if (const std::string* constant = functionConstantNamed(name)) {
			derived.reads.insert(*constant);
		} else if (const auto other = _derivedFunctionConstants.find(name); other != _derivedFunctionConstants.end()
			&& name != declaration.name) {
			derived.reads.insert(other->second.reads.begin(), other->second.reads.end());
		}
	}
}

Id Emitter::derivedFunctionConstantValue(const std::string& name) {
	DerivedFunctionConstant& derived = _derivedFunctionConstants.at(name);
	std::set<std::string>& reads = _functionConstantReads[_helper ? _helper : _entryPoint];
	reads.insert(derived.reads.begin(), derived.reads.end());
	if (derived.value == InvalidId) {
		if (derived.emitting) {
			throw CompileError("the constant \"" + name + "\" is initialised from itself");
		}
		derived.emitting = true;
		const Type& type = derived.declaration->type;
		requireEnumValue("", *derived.declaration->initializer, "the constant \"" + name + "\"");
		const Id value = emitSpecConstantExpression(*derived.declaration->initializer, name);
		derived.value = specConstantConvert(value, _types.scalar(type.scalar));
		derived.emitting = false;
	}
	return derived.value;
}

// A bool or a 32-bit integer specialization constant as another of those types, as an implicit conversion
// does: a bool from the comparison with zero, an integer from a bool as 1 or 0, an int to a uint and back
// keeping the bits (an OpIAdd with zero, as OpBitcast is not one of the operations a shader may use).
Id Emitter::specConstantConvert(Id value, Id type) {
	const Id from = _builder.typeOf(value);
	if (from == type) {
		return value;
	}
	const auto operation = [&](Id resultType, uint16_t op, std::vector<uint32_t> operands) {
		operands.insert(operands.begin(), static_cast<uint32_t>(op));
		return _builder.emitDeclTyped(spirv::OpSpecConstantOp, resultType, operands);
	};
	if (type == _boolType) {
		return operation(_boolType, spirv::OpINotEqual, { value, _types.zero(from) });
	}
	if (from == _boolType) {
		return operation(type, spirv::OpSelect, { value, _types.one(type), _types.zero(type) });
	}
	return operation(type, spirv::OpIAdd, { value, _types.zero(type) });
}

// The initialiser of a derived function constant as specialization constant operations. A name in it is a
// function constant, another derived one, or a constant of the source (an enumerator is a literal by now).
Id Emitter::emitSpecConstantExpression(const Expression& expression, const std::string& constant) {
	const auto refuse = [&]() -> Id {
		throw CompileError("the initialiser of \"" + constant + "\" reads a function constant, and mslc lowers "
			"such an initialiser only with literals, constants and the integer and logical operators");
	};
	const auto operation = [&](Id resultType, uint16_t op, std::vector<uint32_t> operands) {
		operands.insert(operands.begin(), static_cast<uint32_t>(op));
		return _builder.emitDeclTyped(spirv::OpSpecConstantOp, resultType, operands);
	};
	const auto checked = [&](Id value) {
		const Id type = _builder.typeOf(value);
		if (type != _boolType && type != _intType && type != _uintType) {
			refuse();
		}
		return value;
	};
	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
		case ExpressionKind::BoolLiteral:
			return checked(emitExpression(expression));
		case ExpressionKind::Identifier: {
			// The initialiser is file-scope code: a local or parameter of the function that first reads the
			// constant, however it is named, is not in its scope.
			if (_functionConstants.count(expression.name)) {
				return checked(functionConstantValue(expression.name, false));
			}
			const auto alias = _functionConstantDefinedAliases.find(expression.name);
			if (alias != _functionConstantDefinedAliases.end()) {
				return checked(functionConstantValue(alias->second, true));
			}
			if (_derivedFunctionConstants.count(expression.name)) {
				return checked(derivedFunctionConstantValue(expression.name));
			}
			const auto found = _constants.find(expression.name);
			if (found != _constants.end()) {
				return checked(found->second.id);
			}
			return refuse();
		}
		case ExpressionKind::Call:
			if (expression.left && expression.left->kind == ExpressionKind::Identifier
				&& builtinCalleeName(expression.left->name) == "__metal_is_function_constant_defined") {
				return checked(emitIsFunctionConstantDefined(expression));
			}
			return refuse();
		case ExpressionKind::Construct: {
			const ScalarKind target = expression.constructType ? expression.constructType->scalar : ScalarKind::Void;
			if (expression.arguments.size() != 1 || expression.constructType->vectorWidth > 1
				|| (target != ScalarKind::Bool && target != ScalarKind::Int && target != ScalarKind::UInt)) {
				return refuse();
			}
			return specConstantConvert(emitSpecConstantExpression(*expression.arguments[0], constant),
				_types.scalar(target));
		}
		case ExpressionKind::Unary: {
			if (isComponentValue(*expression.left)) {
				throw CompileError("unary operator does not accept a value of scoped enum metal::component without a cast");
			}
			const Id operand = emitSpecConstantExpression(*expression.left, constant);
			switch (expression.unaryOperator) {
				case UnaryOperator::Plus:
					return specConstantConvert(operand, _builder.typeOf(operand) == _boolType ? _intType
						: _builder.typeOf(operand));
				case UnaryOperator::Not:
					return operation(_boolType, spirv::OpLogicalNot, { specConstantConvert(operand, _boolType) });
				case UnaryOperator::Negate:
				case UnaryOperator::BitNot: {
					const Id type = _builder.typeOf(operand) == _boolType ? _intType : _builder.typeOf(operand);
					return operation(type, expression.unaryOperator == UnaryOperator::Negate ? spirv::OpSNegate
						: spirv::OpNot, { specConstantConvert(operand, type) });
				}
				default:
					return refuse();
			}
		}
		case ExpressionKind::Conditional: {
			if (expression.arguments.size() != 2) {
				return refuse();
			}
			checkConditionalOperands(*expression.left, *expression.arguments[0], *expression.arguments[1]);
			const Id condition = specConstantConvert(emitSpecConstantExpression(*expression.left, constant), _boolType);
			Id yes = emitSpecConstantExpression(*expression.arguments[0], constant);
			Id no = emitSpecConstantExpression(*expression.arguments[1], constant);
			if (_builder.typeOf(yes) != _builder.typeOf(no)) {
				// The usual arithmetic conversions: a bool is promoted, and an int beside a uint becomes one.
				const Id type = _builder.typeOf(yes) == _uintType || _builder.typeOf(no) == _uintType
					? _uintType : _intType;
				yes = specConstantConvert(yes, type);
				no = specConstantConvert(no, type);
			}
			return operation(_builder.typeOf(yes), spirv::OpSelect, { condition, yes, no });
		}
		case ExpressionKind::Binary: {
			checkBinaryComponentOperands(expression.binaryOperator, *expression.left, *expression.right);
			const Id left = emitSpecConstantExpression(*expression.left, constant);
			const Id right = emitSpecConstantExpression(*expression.right, constant);
			const BinaryOperator op = expression.binaryOperator;
			if (op == BinaryOperator::LogicalAnd || op == BinaryOperator::LogicalOr) {
				return operation(_boolType, op == BinaryOperator::LogicalAnd ? spirv::OpLogicalAnd
					: spirv::OpLogicalOr, { specConstantConvert(left, _boolType), specConstantConvert(right, _boolType) });
			}
			const bool comparison = op == BinaryOperator::Equal || op == BinaryOperator::NotEqual
				|| op == BinaryOperator::Less || op == BinaryOperator::LessEqual || op == BinaryOperator::Greater
				|| op == BinaryOperator::GreaterEqual;
			if (comparison && _builder.typeOf(left) == _boolType && _builder.typeOf(right) == _boolType
				&& (op == BinaryOperator::Equal || op == BinaryOperator::NotEqual)) {
				return operation(_boolType, op == BinaryOperator::Equal ? spirv::OpLogicalEqual
					: spirv::OpLogicalNotEqual, { left, right });
			}
			// The usual arithmetic conversions: bools are promoted to int, and an int beside a uint becomes one.
			// A shift is the exception: it has the type of its promoted left operand alone.
			const bool shift = op == BinaryOperator::ShiftLeft || op == BinaryOperator::ShiftRight;
			const Id type = shift ? (_builder.typeOf(left) == _uintType ? _uintType : _intType)
				: _builder.typeOf(left) == _uintType || _builder.typeOf(right) == _uintType ? _uintType : _intType;
			const Id a = specConstantConvert(left, type);
			const Id b = specConstantConvert(right, type);
			const bool isSigned = type == _intType;
			if (comparison) {
				return operation(_boolType, comparisonOpcode(op, false, isSigned), { a, b });
			}
			return operation(type, arithmeticOpcode(op, false, isSigned), { a, b });
		}
		default:
			return refuse();
	}
}

// The reflection of the function constants the entry point reads, itself or in a function it reaches, by index.
std::string Emitter::functionConstantsJson() {
	std::map<uint32_t, std::string> byIndex;
	std::set<const FunctionDecl*> reached { _entryPoint };
	std::vector<const FunctionDecl*> pending { _entryPoint };
	while (!pending.empty()) {
		const FunctionDecl* function = pending.back();
		pending.pop_back();
		for (const std::string& name: _functionConstantReads[function]) {
			byIndex[*_functionConstants.at(name).declaration->functionConstantIndex] = name;
		}
		for (const FunctionDecl* callee: _functionCallees[function]) {
			if (reached.insert(callee).second) {
				pending.push_back(callee);
			}
		}
	}
	std::string json;
	for (const auto& [index, name]: byIndex) {
		const VariableDeclaration& declaration = *_functionConstants.at(name).declaration;
		json += std::string(json.empty() ? "" : ",") + "\n\t\t\t\t{ \"name\": \"" + name + "\", \"type\": \""
			+ scalarKindName(declaration.type.scalar) + "\", \"index\": " + std::to_string(index)
			+ ", \"spec_id\": " + std::to_string(kFunctionConstantSpecializationID + index)
			+ ", \"defined_spec_id\": " + std::to_string(kFunctionConstantDefinedSpecializationID + index) + " }";
	}
	return json;
}

// The module's own declarations, in source order. A constant is declared
// before the entry point that reads it, which is what lets the folder resolve
// one constant's initialiser against the ones above it.
void Emitter::declareGlobals() {
	for (size_t position = 0; position < _unit.globals.size(); ++position) {
		const VariableDeclaration& global = _unit.globals[position];
		if (global.sampler) {
			_fileScopeSamplers[global.name] = &global;
			continue;
		}
		_builder.setSection(spirv::Section::TypesGlobals);
		if (global.functionConstantIndex) {
			declareFunctionConstant(global);
			continue;
		}
		if (global.initializer && !_functionConstants.empty()) {
			std::set<std::string> read;
			collectIdentifiers(*global.initializer, read);
			const auto constant = std::find_if(read.begin(), read.end(), [&](const std::string& name) {
					return functionConstantNamed(name) != nullptr || _derivedFunctionConstants.count(name);
				});
			if (constant != read.end()) {
				if (isFunctionConstantDefinedAlias(global)) {
					declareFunctionConstantAlias(global);
				} else {
					declareDerivedFunctionConstant(global);
				}
				continue;
			}
		}
		if (global.type.arrayLength) {
			declareFileScopeArray(global, position);
			continue;
		}
		declareGlobalConstant(global);
	}
}

} // namespace mslc
