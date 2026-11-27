#include "sema/emitter.h"

namespace mslc {

Id Emitter::broadcast(Id value, Id vectorType) {
	// A scalar beside a vector is a broadcast, which is what "v * 2.0" and
	// float3(0) both mean in MSL. The scalar is converted to the vector's own
	// component type and put in every component; converting it to the vector
	// type instead would be an OpFConvert into a vector, which no convert
	// opcode accepts, so an int beside a float vector is converted rather than
	// reinterpreted.
	const Id component = _types.componentOf(vectorType);
	const Id widened = convert(value, _builder.typeOf(value), component);
	return _builder.emitTyped(spirv::OpCompositeConstruct, vectorType,
		std::vector<uint32_t>(_types.vectorWidth(vectorType), widened));
}

// float2x2(1.0f) is the diagonal matrix with 1.0 on it and 0 elsewhere, also for a
// matrix that is not square, and float2x2(m) copies a matrix of the same shape,
// converting its elements (measured with Apple's compiler and a GPU run). A matrix
// of another shape is not a conversion Apple has either.
Id Emitter::emitMatrixFromValue(const Expression& argument, const Type& target, Id toType,
	const std::string& spelled) {
	const Id value = emitNonComponent(argument, "the constructor for " + spelled);
	const Id fromType = _builder.typeOf(value);
	const auto* to = _types.matrixInfo(toType);

	if (const auto* from = _types.matrixInfo(fromType)) {
		if (from->columns != to->columns || from->rows != to->rows) {
			throw CompileError("no matching conversion from a matrix of another shape to " + spelled);
		}
		if (fromType == toType) {
			return value;
		}

		std::vector<uint32_t> columns;
		for (uint32_t column = 0; column < to->columns; ++column) {
			const Id piece = _builder.emitTyped(spirv::OpCompositeExtract, from->column, { value, column });
			columns.push_back(convert(piece, from->column, to->column));
		}
		return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
	}

	if (_types.vectorWidth(fromType) != 1 || _types.bitWidth(fromType) == 0) {
		throw CompileError("no matching conversion from this value to " + spelled
			+ "; a matrix is built from a scalar, a matrix of its own shape, or one vector per column");
	}

	const Id componentType = _types.componentOf(to->column);
	const Id diagonal = convert(value, fromType, componentType);
	const Id zero = _types.zero(componentType);
	std::vector<uint32_t> columns;
	for (uint32_t column = 0; column < to->columns; ++column) {
		std::vector<uint32_t> elements(to->rows, zero);
		if (column < to->rows) {
			elements[column] = diagonal;
		}
		columns.push_back(_builder.emitTyped(spirv::OpCompositeConstruct, to->column, elements));
	}
	return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
}

Id Emitter::emitConstruct(const Expression& expression) {
	if (!expression.constructType) {
		throw CompileError("a constructor needs the type it constructs");
	}

	const Type& target = *expression.constructType;
	const std::string spelled = typeName(target);

	if (isVoidCast(expression)) {
		throw CompileError("a cast to void has no value; it can only be a statement of its own, "
			"as in (void)x;");
	}

	// Only a scalar or a vector is a value a constructor produces. A pointer,
	// an array or a struct is not: "float* p(0)" is a null pointer in C++ and
	// not a pointer built from a value, and a local of a pointer type whose
	// initialiser is a scalar stores that scalar into the pointer, which
	// validates and reads as nonsense.
	if (target.isPointer || target.arrayLength || !target.namedType.empty()) {
		throw CompileError("constructing a " + spelled + " is not lowered yet; "
			"mslc builds a scalar or a vector value");
	}

	// Apple rejects device and constant on a local and takes thread (the function's own
	// storage, so a construct of one is plain) and threadgroup, which needs StorageClass
	// Workgroup. mslc declares these in Function storage, so
	// "threadgroup float3 v(0);" would be a thread-private local in a module
	// whose source says the value is shared, and that validates.
	if (target.addressSpace != AddressSpace::None && target.addressSpace != AddressSpace::Thread) {
		throw CompileError("a local in the " + std::string(addressSpaceName(target.addressSpace))
			+ " address space is not lowered yet; every local mslc declares is "
				"thread-private");
	}

	const Id toType = declaredTypeOf(target);

	// One vector per column, a single scalar for the diagonal, or one matrix of
	// the same shape. A list of scalars filling it element by element is its own
	// lowering and is reported rather than guessed at.
	if (target.isMatrix()) {
		if (expression.arguments.empty()) {
			return _types.zero(toType);
		}

		if (expression.arguments.size() == 1) {
			return emitMatrixFromValue(*expression.arguments.front(), target, toType, spelled);
		}

		// columns * rows scalars fill the columns in order. Apple takes the scalars alone,
		// not mixed with vectors, and converts each to the element type.
		if (expression.arguments.size() == static_cast<size_t>(target.matrixColumns) * target.vectorWidth) {
			const Id componentType = _types.componentOf(_types.matrixInfo(toType)->column);
			std::vector<uint32_t> scalars;
			for (const ExpressionPtr& argument: expression.arguments) {
				const Id value = emitNonComponent(*argument, "the constructor for " + spelled);
				const Id valueType = _builder.typeOf(value);
				if (_types.vectorWidth(valueType) != 1 || _types.bitWidth(valueType) == 0) {
					throw CompileError("no matching constructor for " + spelled + ": " + std::to_string(scalars.size() + 1)
						+ " of its " + std::to_string(expression.arguments.size()) + " values is not a scalar");
				}
				scalars.push_back(convert(value, valueType, componentType));
			}
			std::vector<uint32_t> columns;
			for (uint32_t column = 0; column < target.matrixColumns; ++column) {
				columns.push_back(_builder.emitTyped(spirv::OpCompositeConstruct,
					_types.matrixInfo(toType)->column,
					std::vector<uint32_t>(scalars.begin() + column * target.vectorWidth,
						scalars.begin() + (column + 1) * target.vectorWidth)));
			}
			return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
		}

		if (expression.arguments.size() != target.matrixColumns) {
			throw CompileError("no matching constructor for " + spelled + " built from "
				+ std::to_string(expression.arguments.size()) + " values");
		}

		std::vector<uint32_t> columns;
		for (const ExpressionPtr& argument: expression.arguments) {
			const Id column = emitExpression(*argument);
			if (_builder.typeOf(column) != _types.matrixInfo(toType)->column) {
				throw CompileError("a column of " + spelled + " has to be a "
					+ std::string(scalarKindName(target.scalar))
					+ std::to_string(target.vectorWidth) + " already; mslc converts no column");
			}
			columns.push_back(column);
		}

		return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
	}

	// float3() and float3(0) are different values and mslc has no value to put
	// in the components, so it says so rather than fabricating a zero. xcrun
	// metal accepts the empty form and zero-fills it; a caller cannot tell
	// that apart from float3(0) in the emitted module, which is the reason for
	// the diagnostic.
	if (expression.arguments.empty()) {
		throw CompileError(spelled + "() has no arguments and mslc has no default "
			"value to put in it; write the value you want, as " + spelled + "(0)");
	}

	if (expression.arguments.size() > 1) {
		return emitConstructList(expression, toType);
	}

	const Id value = target.isVector()
		? emitNonComponent(*expression.arguments.front(), "the constructor for " + spelled)
		: emitExpression(*expression.arguments.front());
	const Id fromType = _builder.typeOf(value);

	if (_types.vectorWidth(fromType) == 1 && _types.vectorWidth(toType) > 1) {
		return broadcast(value, toType);
	}

	return convert(value, fromType, toType);
}

// float4(v3, 1), float4(v2, v2), float3(f, v2): the pieces fill the components
// in the order written. Apple's compiler converts a scalar piece but takes a
// vector piece only of the target's own component type, and the widths have to
// add up to the target's; mslc accepts the same and rejects the rest.
Id Emitter::emitConstructList(const Expression& expression, Id toType) {
	const std::string spelled = typeName(*expression.constructType);
	const Id component = _types.componentOf(toType);
	if (component == InvalidId) {
		throw CompileError(spelled + " is built from one value, and this passes "
			+ std::to_string(expression.arguments.size()));
	}

	std::vector<uint32_t> pieces;
	uint32_t components = 0;
	for (const ExpressionPtr& argument: expression.arguments) {
		const Id value = emitNonComponent(*argument, "an argument of " + spelled);
		const Id type = _builder.typeOf(value);

		if (_types.componentOf(type) != InvalidId) {
			if (_types.componentOf(type) != component) {
				throw CompileError("a vector piece of " + spelled + " has to have its "
					"component type; only a scalar piece is converted");
			}
			pieces.push_back(value);
			components += _types.vectorWidth(type);
			continue;
		}

		if (_types.isAggregate(type)) {
			throw CompileError(spelled + " is built from scalars and vectors, not from "
				"a matrix or a struct");
		}

		pieces.push_back(convert(value, type, component));
		components += 1;
	}

	const uint32_t width = _types.vectorWidth(toType);
	if (components != width) {
		throw CompileError(spelled + " built from " + std::to_string(components)
			+ " components needs " + std::to_string(width));
	}

	return _builder.emitTyped(spirv::OpCompositeConstruct, toType, pieces);
}

bool Emitter::canFoldExpression(const Expression& expression, const std::string& excluded) {
	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
		case ExpressionKind::BoolLiteral: return true;
		case ExpressionKind::FloatLiteral: return std::isfinite(foldExpression(expression).number);
		case ExpressionKind::Identifier: {
			if (expression.name == excluded && !expression.fileScope) { return false; }
			const auto local = expression.fileScope ? _bindings.end() : _bindings.find(expression.name);
			if (local != _bindings.end()) { return _localFolded.count(local->second.id) != 0; }
			const auto value = _folded.find(expression.name);
			if (value == _folded.end() || value->second.isComposite
				|| (isFloatKind(value->second.scalar) && !std::isfinite(value->second.number))) { return false; }
			for (const VariableDeclaration& global: _unit.globals) {
				if (global.name != expression.name || !global.initializer) { continue; }
				const Expression* initializer = global.initializer.get();
				while (initializer->kind == ExpressionKind::InitList && !initializer->constructType
					&& initializer->elements.size() == 1 && initializer->elements.front().fieldName.empty()) {
					initializer = initializer->elements.front().value.get();
				}
				if (initializer->kind == ExpressionKind::InitList && !initializer->constructType
					&& initializer->elements.empty()) {
					return true;
				}
				return initializer->kind != ExpressionKind::InitList && canFoldExpression(*initializer, global.name);
			}
			return false;
		}
		case ExpressionKind::Sizeof:
			try {
				sizeofOperand(*expression.left);
				return true;
			} catch (const CompileError&) {
				return false;
			}
		case ExpressionKind::Binary:
		case ExpressionKind::Conditional: return provenByFolding(expression, excluded);
		case ExpressionKind::Call: {
			if (expression.left && expression.left->kind == ExpressionKind::Identifier
				&& expression.left->name == kAsTypeCallee && expression.left->templateArguments.size() == 1
				&& expression.left->templateArguments.front().isType) {
				const Type& type = expression.left->templateArguments.front().type;
				return !type.isVector() && !type.isMatrix() && !type.isPointer
					&& !type.arrayLength && type.namedType.empty() && type.enumName.empty()
					&& expression.arguments.size() == 1 && !isComponentValue(*expression.arguments.front())
					&& canFoldExpression(*expression.arguments.front(), excluded);
			}
			return false;
		}
		case ExpressionKind::Construct: {
			const Type* type = expression.constructType ? &*expression.constructType : nullptr;
			if (type && type->isVector() && !type->isMatrix() && !type->isPointer && !type->arrayLength
				&& type->namedType.empty() && type->enumName.empty()) {
				// An explicit conversion of each component; a float to an integer has a
				// range of its own to check, so that case is left unproven.
				for (const ExpressionPtr& argument: expression.arguments) {
					if (argument->kind == ExpressionKind::Construct && argument->constructType
						&& argument->constructType->isVector()) {
						if (!canFoldExpression(*argument, excluded)) { return false; }
						continue;
					}
					if (!canFoldExpression(*argument, excluded)) { return false; }
					const FoldedConstant value = foldExpression(*argument);
					if (value.isComposite || (isFloatKind(value.scalar) && !isFloatKind(type->scalar))) { return false; }
				}
				return !expression.arguments.empty();
			}
			return provenByFolding(expression, excluded);
		}
		case ExpressionKind::Unary: {
			const UnaryOperator op = expression.unaryOperator;
			if ((op != UnaryOperator::Plus && op != UnaryOperator::Negate
				&& op != UnaryOperator::BitNot && op != UnaryOperator::Not)
				|| !canFoldExpression(*expression.left, excluded)) { return false; }
			const FoldedConstant operand = foldExpression(*expression.left);
			if (isFloatKind(operand.scalar) && op == UnaryOperator::BitNot) { return false; }
			if (op == UnaryOperator::Negate && isSignedInteger(operand.scalar)) {
				const ScalarKind promoted = promotedKind(operand.scalar);
				const uint64_t minimum = normalizeInteger(promoted, uint64_t{ 1 } << (mappingFor(promoted).width - 1));
				if (operand.integer == minimum) { return false; }
			}
			return true;
		}
		default: return false;
	}
}

// What a constant expression is made of: constants at the leaves, and only the
// operators that fold.
bool Emitter::constantLeaves(const Expression& expression, const std::string& excluded) {
	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
		case ExpressionKind::BoolLiteral:
		case ExpressionKind::FloatLiteral:
		case ExpressionKind::Sizeof:
		case ExpressionKind::Identifier: return canFoldExpression(expression, excluded);
		case ExpressionKind::Unary: return constantLeaves(*expression.left, excluded);
		case ExpressionKind::Binary:
			return constantLeaves(*expression.left, excluded) && constantLeaves(*expression.right, excluded);
		case ExpressionKind::Conditional: {
			if (expression.arguments.size() != 2 || !constantLeaves(*expression.left, excluded)) { return false; }
			const bool saved = _foldDomainError;
			bool taken = false;
			try {
				const FoldedConstant condition = foldExpression(*expression.left);
				taken = !condition.isComposite && (condition.scalar == ScalarKind::Bool ? condition.boolean
					: isFloatKind(condition.scalar) ? condition.number != 0.0 : condition.integer != 0);
			} catch (const CompileError&) {
				_foldDomainError = saved;
				return false;
			}
			_foldDomainError = saved;
			return constantLeaves(*expression.arguments[taken ? 0 : 1], excluded);
		}
		case ExpressionKind::Call:
			return expression.left && expression.left->kind == ExpressionKind::Identifier
				&& expression.left->name == kAsTypeCallee && expression.arguments.size() == 1
				&& constantLeaves(*expression.arguments.front(), excluded);
		case ExpressionKind::Construct:
			for (const ExpressionPtr& argument: expression.arguments) {
				if (!constantLeaves(*argument, excluded)) { return false; }
			}
			return !expression.arguments.empty();
		default: return false;
	}
}

// Folds an expression of constants and says whether it is one: an operation no constant
// expression holds (a signed overflow, a shift out of range, a division by zero) or a
// form the folder does not know is not proven.
bool Emitter::provenByFolding(const Expression& expression, const std::string& excluded) {
	if (!constantLeaves(expression, excluded)) { return false; }
	const bool saved = _foldDomainError;
	_foldDomainError = false;
	bool proven = true;
	try {
		foldExpression(expression);
	} catch (const CompileError&) {
		proven = false;
	}
	proven = proven && !_foldDomainError;
	_foldDomainError = saved;
	return proven;
}

Id Emitter::convertListScalar(Id value, const Type& target, const Expression& expression) {
	requireEnumValue(target.enumName, expression, "an element of the initializer list");
	const auto from = _types.scalarKindOf(_builder.typeOf(value));
	if (!from) { throw CompileError("a scalar initializer list needs a scalar value"); }
	const ScalarKind to = target.scalar;
	const bool integerWidening = !isFloatKind(*from) && !isFloatKind(to)
		&& (isSignedInteger(*from) == isSignedInteger(to)
			? mappingFor(*from).width <= mappingFor(to).width
			: !isSignedInteger(*from) && mappingFor(*from).width < mappingFor(to).width);
	if (*from != to && !(*from == ScalarKind::Half && to == ScalarKind::Float) && !integerWidening) {
		if (isFloatKind(*from) && !isFloatKind(to)) {
			throw CompileError("a floating value cannot be narrowed to an integer or bool in an initializer list");
		}
		if (!canFoldExpression(expression)) {
			throw CompileError("a narrowing initializer list conversion requires a supported constant expression; this form is not lowered yet");
		}
		FoldedConstant folded = foldExpression(expression);
		if (folded.scalar == ScalarKind::Bool) { folded.integer = folded.boolean ? 1 : 0; }
		requireNotNarrowing(folded, to);
	}
	return convertImplicit(value, _types.scalar(to));
}

void Emitter::discardLifted(const std::function<void()>& emit) {
	const size_t mark = _builder.functionsMark();
	const size_t locals = _builder.localsMark();
	const Id block = _currentBlock;
	const bool terminated = _terminated;
	const uint32_t depth = _controlDepth;
	const size_t targets = _jumpTargets.size();
	// what the lifted code reads is not read
	const auto functionConstantReads = _functionConstantReads;
	const auto numSamplesReaders = _numSamplesReaders;
	const auto deducedBefore = _deducedReturns;
	emit();
	_builder.takeFunctionsFrom(mark);
	_builder.truncateLocals(locals);
	// An auto function first called in the lifted code was emitted whole and is kept (deduceReturnType put
	// its body aside), and it is not emitted again: what it reads stays read.
	std::map<const FunctionDecl*, std::set<std::string>> keptReads;
	std::set<const FunctionDecl*> keptReaders;
	for (const auto& [function, type]: _deducedReturns) {
		if (deducedBefore.count(function)) {
			continue;
		}
		if (const auto reads = _functionConstantReads.find(function); reads != _functionConstantReads.end()) {
			keptReads.insert(*reads);
		}
		if (_numSamplesReaders.count(function)) {
			keptReaders.insert(function);
		}
	}
	_functionConstantReads = functionConstantReads;
	_numSamplesReaders = numSamplesReaders;
	for (auto& [function, reads]: keptReads) {
		_functionConstantReads[function].insert(reads.begin(), reads.end());
	}
	_numSamplesReaders.insert(keptReaders.begin(), keptReaders.end());
	// A short-circuit or ?: inside the element moved these to a label that the lift removed.
	_currentBlock = block;
	_terminated = terminated;
	_controlDepth = depth;
	_jumpTargets.resize(targets);
}

// Apple type-checks an ignored initializer element but never evaluates it, so
// it is emitted for its diagnostics and the instructions are lifted out again.
void Emitter::checkAndDiscard(const Expression& expression) {
	if (expression.kind == ExpressionKind::InitList) {
		for (const InitializerElement& element: expression.elements) { checkAndDiscard(*element.value); }
		return;
	}
	discardLifted([&] { emitExpression(expression); });
}

Id Emitter::emitListScalarValue(const Type& target, const Expression& expression, bool dropExcess) {
	if (expression.kind != ExpressionKind::InitList) {
		return convertListScalar(emitExpression(expression), target, expression);
	}
	if (expression.elements.empty()) { return _types.zero(declaredTypeOf(target)); }
	if (expression.elements.size() != 1 && !dropExcess) {
		throw CompileError("nested scalar initialization with excess elements is not lowered yet");
	}
	if (!expression.elements.front().fieldName.empty()) {
		throw CompileError("a field designator requires a struct initializer");
	}
	Id first = emitListScalarValue(target, *expression.elements.front().value, dropExcess);
	for (size_t i = 1; i < expression.elements.size(); ++i) { checkAndDiscard(*expression.elements[i].value); }
	return first;
}

// Apple keeps the last initializer of each member, drops the earlier ones unevaluated (but still
// type-checked) and evaluates the survivors in member order, whatever order they were written in.
// A flat cursor over the element list places each element; a struct member that is reached by an
// element which is neither braced nor already of the member's struct type takes the following
// elements too (brace elision).
void Emitter::planStructElements(const StructDecl& decl, const std::vector<InitializerElement>& elements,
		size_t& cursor, std::vector<std::optional<Id>>& values, bool elided, InitPlan& plan,
		std::vector<DroppedInit>& dropped) {
	const std::string spelled = decl.name;
	plan.decl = &decl;
	plan.slots.resize(decl.fields.size());
	size_t next = 0;
	while (cursor < elements.size()) {
		const InitializerElement& element = elements[cursor];
		if (!element.fieldName.empty()) {
			if (elided) { break; }
			next = 0;
			while (next < decl.fields.size() && decl.fields[next].name != element.fieldName) { ++next; }
			if (next == decl.fields.size()) {
				throw CompileError("field " + element.fieldName + " is unknown in " + spelled + " initializer");
			}
		}
		if (next == decl.fields.size()) {
			if (elided) { break; }
			throw CompileError("excess elements in " + spelled + " initializer");
		}
		const Type& field = decl.fields[next].type;
		if (field.arrayLength && element.value->kind != ExpressionKind::InitList) {
			throw CompileError("brace elision into the array member " + decl.fields[next].name + " of " + spelled
				+ " is not lowered yet; add the braces");
		}
		InitSlot slot;
		slot.index = cursor;
		const StructDecl* fieldDecl = field.namedType.empty() || field.arrayLength.has_value()
			|| element.value->kind == ExpressionKind::InitList || !element.fieldName.empty()
			? nullptr : _unit.findStruct(field.namedType);
		if (fieldDecl && !fieldDecl->hasConstructors) {
			Id sourceType = InvalidId;
			const ExpressionKind kind = element.value->kind;
			const bool literal = kind == ExpressionKind::IntLiteral || kind == ExpressionKind::FloatLiteral
				|| kind == ExpressionKind::BoolLiteral;
			if (values[cursor]) { sourceType = _builder.typeOf(*values[cursor]); }
			else if (!literal) { discardLifted([&] { sourceType = _builder.typeOf(emitExpression(*element.value)); }); }
			const std::string* sourceName = _types.structNameOf(sourceType, nullptr);
			if (!(sourceName && *sourceName == field.namedType)) {
				if (fieldDecl->fields.empty()) {
					throw CompileError("initializer for " + typeName(field) + ", which has no members, requires explicit braces");
				}
				if (std::any_of(fieldDecl->fields.begin(), fieldDecl->fields.end(),
						[](const StructField& member) { return member.type.arrayLength.has_value(); })) {
					throw CompileError("brace elision into " + typeName(field) + ", which has an array member, "
						"is not lowered yet; add the braces");
				}
				slot.group = std::make_shared<InitPlan>();
				planStructElements(*fieldDecl, elements, cursor, values, true, *slot.group, dropped);
			}
		}
		if (!slot.group) {
			slot.element = &element;
			++cursor;
		}
		if (plan.slots[next].element || plan.slots[next].group) {
			dropped.push_back({plan.slots[next], &field});
		}
		plan.slots[next] = std::move(slot);
		++next;
	}
}

Id Emitter::emitInitSlot(const Type& field, const InitSlot& slot, std::vector<std::optional<Id>>& values) {
	if (slot.group) { return emitInitPlan(field, *slot.group, values); }
	const Expression& expression = *slot.element->value;
	if (expression.kind == ExpressionKind::InitList) { return emitInitListValue(field, expression); }
	if (!field.isPointer && !field.arrayLength) {
		requireEnumValue(field.enumName, expression, "an element of the initializer list");
	}
	if (!values[slot.index]) { values[slot.index] = emitExpression(expression); }
	const Id value = *values[slot.index];
	return field.namedType.empty() && field.isScalar()
		? convertListScalar(value, field, expression)
		: convertImplicit(value, declaredTypeOf(field));
}

Id Emitter::emitInitPlan(const Type& target, const InitPlan& plan, std::vector<std::optional<Id>>& values) {
	Expression empty;
	empty.kind = ExpressionKind::InitList;
	std::vector<uint32_t> fields;
	for (size_t i = 0; i < plan.decl->fields.size(); ++i) {
		const Type& field = plan.decl->fields[i].type;
		const InitSlot& slot = plan.slots[i];
		fields.push_back(slot.element || slot.group ? emitInitSlot(field, slot, values) : emitInitListValue(field, empty));
	}
	return composeStruct(declaredTypeOf(target), fields);
}

Id Emitter::emitStructElements(const Type& target, const StructDecl& decl,
		const std::vector<InitializerElement>& elements, std::vector<std::optional<Id>>& values) {
	size_t cursor = 0;
	InitPlan plan;
	std::vector<DroppedInit> dropped;
	planStructElements(decl, elements, cursor, values, false, plan, dropped);
	const Id composite = emitInitPlan(target, plan, values);
	for (const DroppedInit& drop: dropped) {
		discardLifted([&] { emitInitSlot(*drop.field, drop.slot, values); });
	}
	return composite;
}

Id Emitter::emitInitListValue(const Type& target, const Expression& list) {
	if (target.addressSpace != AddressSpace::None) {
		throw CompileError("a local in the " + std::string(addressSpaceName(target.addressSpace))
			+ " address space is not lowered yet; every local mslc declares is thread-private");
	}
	if (target.arrayLength) { return emitArrayListValue(target, list); }
	const Id toType = declaredTypeOf(target);
	const std::string spelled = typeName(target);
	if (!target.namedType.empty()) {
		const StructDecl& decl = *_unit.findStruct(target.namedType);
		if (decl.hasConstructors) {
			throw CompileError("brace initialization of " + spelled + " with declared constructors is not lowered yet");
		}
		const bool hasArrayMember = std::any_of(decl.fields.begin(), decl.fields.end(),
			[](const StructField& field) { return field.type.arrayLength.has_value(); });
		if (hasArrayMember && list.elements.empty()) {
			return _types.zero(toType);
		}
		std::vector<std::optional<Id>> values(list.elements.size());
		// S s{S{1,2}} copies the temporary; a bare {1,2} there would initialise the first member.
		const Expression* only = list.elements.size() == 1 ? list.elements.front().value.get() : nullptr;
		if (only && list.elements.front().fieldName.empty() && only->kind == ExpressionKind::InitList
			&& only->constructType && only->constructType->namedType == target.namedType) {
			return convertImplicit(emitExpression(*only), toType);
		}
		if (list.elements.size() == 1 && list.elements.front().fieldName.empty()
			&& list.elements.front().value->kind != ExpressionKind::InitList) {
			values[0] = emitExpression(*list.elements.front().value);
			const std::string* sourceName = _types.structNameOf(_builder.typeOf(*values[0]), nullptr);
			if (sourceName && *sourceName == target.namedType) {
				return convertImplicit(*values[0], toType);
			}
		}
		return emitStructElements(target, decl, list.elements, values);
	}
	if (list.elements.empty()) { return _types.zero(toType); }
	for (const InitializerElement& element: list.elements) {
		if (!element.fieldName.empty()) { throw CompileError("a field designator requires a struct initializer"); }
	}
	if (target.isMatrix()) {
		if (list.elements.size() > target.matrixColumns) {
			throw CompileError("excess elements in a matrix initializer; Apple ignores them, "
				"mslc asks for one per column");
		}
		Type columnType;
		columnType.scalar = target.scalar;
		columnType.vectorWidth = target.vectorWidth;
		const Id columnId = declaredTypeOf(columnType);
		std::vector<uint32_t> columns;
		for (const InitializerElement& element: list.elements) {
			if (element.value->kind == ExpressionKind::InitList && element.value->elements.empty()) {
				throw CompileError("an empty matrix column initializer is not lowered yet");
			}
			if (element.value->kind == ExpressionKind::InitList
				&& element.value->elements.size() != target.vectorWidth) {
				throw CompileError("a column of " + typeName(target) + " takes "
					+ std::to_string(target.vectorWidth) + " values in braces");
			}
			Id column = element.value->kind == ExpressionKind::InitList
				? emitInitListValue(columnType, *element.value) : emitExpression(*element.value);
			// A scalar fills the whole column, as it does in Apple's compiler.
			if (_types.scalarKindOf(_builder.typeOf(column)) && element.value->kind != ExpressionKind::InitList) {
				Type component = columnType;
				component.vectorWidth = 0;
				column = broadcast(convertListScalar(column, component, *element.value), columnId);
			}
			if (_builder.typeOf(column) != columnId) {
				throw CompileError("a matrix initializer column has to have its column type already");
			}
			columns.push_back(column);
		}
		while (columns.size() < target.matrixColumns) {
			columns.push_back(_types.zero(columnId));
		}
		return _builder.emitTyped(spirv::OpCompositeConstruct, toType, columns);
	}
	Type component;
	component.scalar = target.scalar;
	component.enumName = target.enumName;
	if (target.isScalar()) {
		if (list.elements.size() != 1) { throw CompileError("excess elements in a scalar initializer list"); }
		return emitListScalarValue(component, *list.elements.front().value, true);
	}
	std::vector<uint32_t> pieces;
	uint32_t width = 0;
	for (const InitializerElement& element: list.elements) {
		Id value = element.value->kind == ExpressionKind::InitList
			? emitListScalarValue(component, *element.value) : emitExpression(*element.value);
		const Id valueType = _builder.typeOf(value);
		if (_types.scalarKindOf(valueType)) {
			if (element.value->kind != ExpressionKind::InitList) { value = convertListScalar(value, component, *element.value); }
			++width;
		} else {
			if (list.elements.size() == 1 && valueType == toType) { return value; }
			throw CompileError("a vector initializer list needs scalar elements or one value of its exact vector type");
		}
		pieces.push_back(value);
	}
	if (width > target.vectorWidth) { throw CompileError("a vector initializer list has too many components"); }
	while (pieces.size() < target.vectorWidth) { pieces.push_back(_types.zero(_types.scalar(component.scalar))); }
	return _builder.emitTyped(spirv::OpCompositeConstruct, toType, pieces);
}

} // namespace mslc
