#include "sema/emitter.h"

namespace mslc {

// A label's id is allocated ahead of time so a branch can name it, which
// is why this emits the label with its own id instead of emitDecl's.
void Emitter::beginBlock(Id label) {
	_builder.emit(spirv::OpLabel, { label });
	_currentBlock = label;
	_terminated = false;
}

void Emitter::terminate(uint16_t opcode, std::vector<uint32_t> operands) {
	_builder.emit(opcode, std::move(operands));
	_terminated = true;
}

// A block that already returned must not also branch to the merge.
void Emitter::branchUnlessTerminated(Id label) {
	if (!_terminated) {
		terminate(spirv::OpBranch, { label });
	}
}

// The left side of an assignment is a place, so it is emitted as an address
// rather than loaded.
Id Emitter::emitPlaceAddress(const Expression& left) {
	Id address = InvalidId;
	if (left.kind == ExpressionKind::Index) {
		address = emitIndex(left, true);
	} else if (left.kind == ExpressionKind::Member) {
		if (pointerMemberType(left)) {
			throw CompileError("the pointer member \"" + left.memberName + "\" is only lowered "
				"when indexed or dereferenced, as in s.p[i] or *s.p");
		}
		Id fieldType = InvalidId;
		address = emitMemberAddress(left, fieldType);
	} else if (left.kind == ExpressionKind::Identifier) {
		// A local names a Function-storage pointer, so the variable itself is
		// the address to store to. A buffer parameter is a descriptor and a
		// plain value is loaded, so only this case takes the binding.
		const Binding* it = findBinding(left);
		if (it && it->pointeeMsl.isAtomic) {
			throw CompileError("the atomic \"" + left.name + "\" cannot be used as an ordinary place; "
				"use the atomic functions");
		}
		if (it && it->isPointer
			&& it->bufferPointeeType == InvalidId
			&& (it->storageClass == spirv::StorageClass::Function
				|| it->storageClass == spirv::StorageClass::Workgroup
				|| it->storageClass == spirv::StorageClass::PhysicalStorageBuffer)) {
			address = it->id;
		} else if (it && it->bufferPointeeType != InvalidId
			&& !it->isBuffer) {
			// A reference parameter is the one value its buffer holds.
			address = bufferBase(*it);
		} else {
			address = emitExpression(left);
		}
	} else if (left.kind == ExpressionKind::Conditional) {
		throw CompileError("assigning to a conditional expression is not supported; "
			"assign inside an if instead");
	} else if (left.kind == ExpressionKind::Comma) {
		throw CompileError("assigning to a comma expression is not lowered yet; "
			"assign to its right operand on its own");
	} else {
		address = emitExpression(left);
	}

	return address;
}

// v.xy = e, s.pos.zw = e, buf[i].w = e: the lanes the swizzle names are
// replaced in the whole vector and the vector is stored back, so the lanes it
// does not name keep their value. The address is computed once.
void Emitter::emitSwizzleStore(const Expression& target, const Expression& valueExpression,
	std::optional<BinaryOperator> compound) {
	const std::string& name = target.memberName;
	const std::string quoted = "\"." + name + "\"";
	const std::vector<uint32_t> lanes = swizzleLanes(name);
	for (size_t i = 0; i < lanes.size(); ++i) {
		for (size_t j = i + 1; j < lanes.size(); ++j) {
			if (lanes[i] == lanes[j]) {
				throw CompileError("assigning to " + quoted + " names a component twice");
			}
		}
	}

	const Expression& base = *target.left;
	if (base.kind == ExpressionKind::Member && !structOf(*base.left)) {
		throw CompileError("assigning to " + quoted + " of a swizzle is not lowered yet");
	}

	const Expression* root = &storeRoot(base);
	if (readOnlyRoot(*root)) {
		throw CompileError("assigning to " + quoted + " of \"" + (root->kind == ExpressionKind::Member
			? root->memberName : root->name) + "\", which is const or in constant memory");
	}

	const Id address = emitPlaceAddress(base);
	const Id addressType = _builder.typeOf(address);
	const Id vectorType = valueTypeAt(address);
	const auto storageClass = _types.storageClassOf(addressType);
	if (!storageClass) {
		throw CompileError("assigning to " + quoted + " of a value that is not a local, "
			"a struct member or a buffer element");
	}
	const bool inBuffer = *storageClass == spirv::StorageClass::PhysicalStorageBuffer;
	if (_types.componentOf(vectorType) == InvalidId) {
		throw CompileError("assigning to " + quoted + " of a value that is not a vector");
	}
	requireLanesWithin(name, lanes, _types.vectorWidth(vectorType));

	Id value = emitExpression(valueExpression);
	const Id component = _types.componentOf(vectorType);
	const auto count = static_cast<uint32_t>(lanes.size());

	const Id old = inBuffer ? loadFromBuffer(address, vectorType) : loadFrom(address, vectorType);
	if (compound) {
		// "v.xy += e" is "v.xy = v.xy + e": the lanes the swizzle names are
		// read out of the one load, combined, and written back by the same
		// shuffle a plain store uses.
		const Id lanesType = _types.withWidth(vectorType, count);
		const Id current = count == 1
			? _builder.emitTyped(spirv::OpCompositeExtract, component, { old, lanes[0] })
			: _builder.emitTyped(spirv::OpVectorShuffle, lanesType, [&] {
				std::vector<uint32_t> operands = { old, old };
				operands.insert(operands.end(), lanes.begin(), lanes.end());
				return operands;
			}());
		value = emitCompoundOperation(*compound, current, value);
	}
	const Id valueType = _builder.typeOf(value);
	Id updated = InvalidId;
	if (count == 1) {
		if (_types.vectorWidth(valueType) != 1) {
			throw CompileError("assigning a vector of " + std::to_string(_types.vectorWidth(valueType))
				+ " components to " + quoted + ", which names one");
		}
		updated = _builder.emitTyped(spirv::OpCompositeInsert, vectorType,
			{ convert(value, valueType, component), old, lanes[0] });
	} else {
		const Id lanesType = _types.withWidth(vectorType, count);
		if (_types.vectorWidth(valueType) == 1) {
			value = broadcast(value, lanesType);
		} else if (valueType != lanesType) {
			throw CompileError("assigning to " + quoted + " takes a vector of " + std::to_string(count)
				+ " components of the same type as the target, or a scalar");
		}

		// Lane j keeps the old vector's component unless the swizzle names it,
		// in which case it takes the matching component of the value.
		std::vector<uint32_t> operands = { old, value };
		for (uint32_t lane = 0; lane < _types.vectorWidth(vectorType); ++lane) {
			uint32_t source = lane;
			for (uint32_t k = 0; k < count; ++k) {
				if (lanes[k] == lane) {
					source = _types.vectorWidth(vectorType) + k;
				}
			}
			operands.push_back(source);
		}
		updated = _builder.emitTyped(spirv::OpVectorShuffle, vectorType, operands);
	}

	if (inBuffer) {
		storeIntoBuffer(address, updated);
	} else {
		_builder.emit(spirv::OpStore, { address, updated });
	}
}

void Emitter::emitExpressionStatement(const Expression& expression) {
	// (void)e and static_cast<void>(e): e is evaluated for its effects and its
	// value dropped, so an assignment or a call in it still happens.
	if (isVoidCast(expression)) {
		// (void)param; is the idiom for an unused parameter: reading a texture, a
		// sampler or a buffer name has no value to evaluate, so naming a parameter is
		// all there is to do. An undeclared name is still reported.
		const Expression& operand = *expression.arguments.front();
		if (operand.kind == ExpressionKind::Identifier
			&& findResourceBinding(operand.name, operand.fileScope)) {
			return;
		}
		emitExpressionStatement(operand);
		return;
	}

	if (expression.kind == ExpressionKind::Comma) {
		emitExpressionStatement(*expression.left);
		emitExpressionStatement(*expression.right);
		return;
	}

	// c ? (void)a : (void)b has no value, so it runs as the if it stands for.
	if (expression.kind == ExpressionKind::Conditional && isVoidValued(expression)) {
		emitVoidConditional(expression, nullptr);
		return;
	}

	// ++x and x++ as a statement are x += 1; their value is not lowered.
	if (expression.kind == ExpressionKind::Unary && isStep(expression.unaryOperator)) {
		if (const auto stepped = enumOf(*expression.left); stepped && !stepped->empty()) {
			throw CompileError("++ and -- do not apply to the enum " + *stepped
				+ "; Apple does not convert an integer back to it");
		}
		// C++17 removed ++ on a bool and never had --, and Apple refuses both; the compound path below
		// would take one as b += 1.
		if (const std::optional<Type> target = placeType(*expression.left);
			target && target->scalar == ScalarKind::Bool && !target->isPointer) {
			throw CompileError("++ and -- do not apply to a bool");
		}
		Expression one;
		one.kind = ExpressionKind::IntLiteral;
		one.intValue = 1;
		one.line = expression.line;
		const bool increment = expression.unaryOperator == UnaryOperator::PreIncrement
			|| expression.unaryOperator == UnaryOperator::PostIncrement;
		emitAssignment(*expression.left, one,
			increment ? BinaryOperator::Add : BinaryOperator::Subtract);
		return;
	}

	// A call whose value is dropped, which is the only way to call a void helper.
	if (expression.kind == ExpressionKind::Call && isAsTypeCall(expression)) {
		emitAsType(expression);
		return;
	}
	if (expression.kind == ExpressionKind::Call && emitVoidCall(expression, nullptr)) {
		return;
	}

	if (expression.kind == ExpressionKind::Index && indexOperatorOwner(expression)) { emitIndex(expression, false, true); return; }

	// An assignment yields an address rather than a value, so it is
	// handled here rather than through emitExpression.
	if (expression.kind != ExpressionKind::Assign) {
		emitExpression(expression);
		return;
	}

	emitAssignment(*expression.left, *expression.right, expression.compoundOperator);
}

// The variable a store lands in: the head of a chain of member and element
// accesses, since constness and storage follow the chain. A pointer member,
// as s.p in s.p[i], ends the chain: what it points at is not part of s.
const Expression& Emitter::storeRoot(const Expression& target) {
	const Expression* root = &target;
	while ((root->kind == ExpressionKind::Member && !pointerMemberType(*root))
		|| root->kind == ExpressionKind::Index) {
		root = root->left.get();
	}
	return *root;
}

// Whether a store through a place's root is refused: a const or constant
// variable, or a pointer member to const or constant memory.
bool Emitter::readOnlyRoot(const Expression& root) {
	if (const Type* pointer = pointerMemberType(root)) {
		return pointer->isConst || pointer->addressSpace == AddressSpace::Constant;
	}
	if (root.kind != ExpressionKind::Identifier) {
		return false;
	}
	if (const Binding* binding = findBinding(root)) {
		return binding->readOnly;
	}
	return _constants.count(root.name) > 0 || functionConstantNamed(root.name) != nullptr;
}

// A store to something that is not a variable of the entry point: a literal,
// which is what an enumerator is by now, or a file-scope constant.
void Emitter::requireStorable(const Expression& target) {
	const Expression& root = storeRoot(target);
	if (root.kind == ExpressionKind::IntLiteral || root.kind == ExpressionKind::FloatLiteral
		|| root.kind == ExpressionKind::BoolLiteral) {
		throw CompileError("cannot assign to a literal or an enumerator constant");
	}
	if (isTemporaryPart(target)) {
		throw CompileError("expression is not assignable");
	}
	rejectPlaceThroughConditional(target, "assigning to");
	if (root.kind == ExpressionKind::Identifier && !findBinding(root)
		&& (_constants.count(root.name) || functionConstantNamed(root.name))) {
		throw CompileError("cannot store to \"" + root.name + "\", a constant declared at file scope");
	}
}

void Emitter::emitAssignment(const Expression& left, const Expression& right,
	std::optional<BinaryOperator> compound) {
	requireStorable(left);
	// A struct prvalue takes the trivial assignment: the right side is evaluated first, as
	// Apple does, then the left, and the result is dropped.
	if (left.kind == ExpressionKind::InitList || left.kind == ExpressionKind::Call) {
		const StructDecl* target = structOf(left);
		if (!target) {
			throw CompileError("expression is not assignable");
		}
		if (compound) {
			throw CompileError("no viable overloaded compound assignment for a " + target->name + " temporary");
		}
		const Id value = emitExpression(right);
		const std::string* source = _types.structNameOf(_builder.typeOf(value), nullptr);
		if (!source || *source != target->name) {
			throw CompileError("no viable overloaded '=' for a " + target->name + " temporary");
		}
		emitExpression(left);
		return;
	}
	if (left.kind == ExpressionKind::Identifier) {
		const Binding* named = findResourceBinding(left.name, left.fileScope);
		if (named && named->pointeeMsl.resource != ResourceKind::None
			&& !named->pointeeMsl.arrayLength) {
			throw CompileError("\"" + left.name + "\" is a " + typeName(named->pointeeMsl)
				+ ", which cannot be assigned: mslc binds a texture or sampler name to one descriptor "
				"and does not lower choosing another at run time");
		}
		if (named && named->pointeeMsl.arrayLength && !named->isBuffer
			&& !(named->pointeeMsl.isStdArray && isLocalStorage(named->storageClass))) {
			const bool unsized = *named->pointeeMsl.arrayLength == UINT32_MAX;
			throw CompileError(unsized ? "the threadgroup argument \"" + left.name + "\" is not assignable; assign through it"
				: "array type " + typeName(named->pointeeMsl) + " is not assignable");
		}
	}
	std::optional<std::string> target = enumOf(left);
	if (!target) {
		if (left.kind == ExpressionKind::Member && !structOf(*left.left)) {
			target = std::string();
		} else if (const std::optional<Type> place = placeType(left);
			place && !place->isPointer && !place->arrayLength) {
			target = place->enumName;
		}
	}
	if (target && *target != "<anonymous>") {
		if (!target->empty() && compound) {
			throw CompileError("a compound assignment does not apply to the enum " + *target
				+ "; Apple does not convert the integer result back to it");
		}
		requireEnumValue(*target, right, "the target of the assignment");
	} else if (!target) {
		requireEnumValue("", right, "the target of the assignment");
	}

	if (left.kind == ExpressionKind::Member
		&& !structOf(*left.left)) {
		emitSwizzleStore(left, right, compound);
		return;
	}

	const Expression* root = &storeRoot(left);
	if (readOnlyRoot(*root)) {
		throw CompileError("cannot store through \"" + (root->kind == ExpressionKind::Member
			? root->memberName : root->name) + "\", which is in the "
			"constant address space or declared const");
	}

	const Id address = emitPlaceAddress(left);

	// A store's value has the type the address points at, not the type of
	// the address, so the pointee is what the value is converted to.
	const Id addressType = _builder.typeOf(address);
	const Id pointeeType = valueTypeAt(address);
	if (pointeeType == spirv::InvalidId) {
		throw CompileError("cannot determine what \""
			+ left.name + "\" points at, so the store cannot be typed");
	}

	// A store into a buffer carries the Aligned memory operand, which the
	// same layout rule gives the access.
	const auto storageClass = _types.storageClassOf(addressType);

	Id value = InvalidId;
	if (compound) {
		const bool inBuffer = storageClass
			&& *storageClass == spirv::StorageClass::PhysicalStorageBuffer;
		const Id current = inBuffer
			? loadFromBuffer(address, pointeeType) : loadFrom(address, pointeeType);
		value = emitCompoundOperation(*compound, current,
			emitExpression(right));
	} else if (right.kind == ExpressionKind::InitList && !right.constructType) {
		const std::optional<Type> target = placeType(left);
		if (!target || !target->isStdArray || !target->arrayLength || target->isPointer) {
			throw CompileError("assigning a braced list is lowered to an array<T, N> only");
		}
		value = emitInitListValue(*target, right);
	} else {
		value = emitExpression(right);
	}

	if (storageClass && *storageClass == spirv::StorageClass::PhysicalStorageBuffer) {
		storeIntoBuffer(address, convertImplicit(value, pointeeType));
		return;
	}

	_builder.emit(spirv::OpStore, { address, convertImplicit(value, pointeeType) });
}

// What stands for the value of a constexpr that is never reached: only that it
// is a constant matters to the declarations after it, not what it holds.
static FoldedConstant placeholderConstant(const Type& type) {
	FoldedConstant constant;
	constant.scalar = type.scalar;
	return constant;
}

// A switch is a selection: OpSelectionMerge names the block after the body,
// and OpSwitch sends the selector to one case's block, each of which
// branches to the next for a fall-through or, through a break, to the merge.
// A switch with no default sends every unmatched value to the merge.
void Emitter::emitSwitch(const Statement& statement) {
	const bool selectorComponent = isComponentValue(*statement.expression);
	Id selector = emitExpression(*statement.expression);
	const Id emittedType = _builder.typeOf(selector);

	// The selector arrives as a value, so its MSL type is recovered from the
	// SPIR-V one. A bool is an integer type C++ accepts here; OpSwitch does
	// not, so it switches on the bool's int value, 1 or 0.
	const std::optional<ScalarKind> selectorKind = _types.scalarKindOf(emittedType);
	if (!selectorKind || isFloatKind(*selectorKind)) {
		const auto diagnosticKind = selectorKind ? selectorKind
			: _types.scalarKindOf(_types.componentOf(emittedType));
		std::string name = diagnosticKind ? std::string(scalarKindName(*diagnosticKind)) : "void";
		if (diagnosticKind && _types.vectorWidth(emittedType) > 1) {
			name += std::to_string(_types.vectorWidth(emittedType));
		}
		throw CompileError("statement requires expression of integer type ('" + name + "' invalid)");
	}

	const ScalarKind kind = promotedKind(*selectorKind);
	if (kind != *selectorKind) {
		selector = convert(selector, emittedType, _intType);
	}

	const Id mergeLabel = _builder.nextId();
	Id defaultLabel = InvalidId;
	std::vector<Id> labels(statement.switchCases.size());
	std::vector<uint64_t> literals(statement.switchCases.size(), 0);

	for (Id& label: labels) {
		label = _builder.nextId();
	}

	std::set<uint64_t> seen;
	{
		const BindingScope validationScope(_bindings);
		for (const StatementPtr& child: statement.switchPreamble) {
			if (child->kind == StatementKind::DeclarationStatement) {
				const auto& declaration = *child->declaration;
				if (!statement.switchCases.empty() && (declaration.initializer
					|| declaration.type.resource == ResourceKind::Sampler)) {
					throw CompileError("switch label bypasses variable initialization");
				}
				_bindings[declaration.name] = Binding{};
			}
		}
		for (size_t i = 0; i < statement.switchCases.size(); ++i) {
			const SwitchCase& kase = statement.switchCases[i];
			if (!kase.value) {
				if (defaultLabel != InvalidId) {
					throw CompileError("multiple default labels in one switch");
				}
				defaultLabel = labels[i];
			} else {
				if (selectorComponent != isComponentValue(*kase.value)) {
					throw CompileError(selectorComponent
						? "a switch over metal::component requires case labels of type metal::component"
						: "a value of scoped enum metal::component cannot be used as a case label for a non-component switch");
				}
				FoldedConstant folded;
				try {
					folded = foldExpression(*kase.value);
				} catch (const CompileError&) {
					throw CompileError("case value is not a constant expression");
				}
				if (folded.isComposite || isFloatKind(folded.scalar)) {
					throw CompileError("case value is not an integer constant expression");
				}
				// A bool case value is its int value, as the selector is.
				if (folded.scalar == ScalarKind::Bool) {
					folded.scalar = ScalarKind::Int;
					folded.integer = folded.boolean ? 1 : 0;
				}

				if (!fitsInKind(folded, kind)) {
					throw CompileError("case value evaluates to " + integerText(folded)
						+ ", which cannot be narrowed to type '" + scalarKindName(kind) + "'");
				}

				literals[i] = normalizeInteger(kind, folded.integer);
				if (!seen.insert(literals[i]).second) {
					FoldedConstant asSelector;
					asSelector.scalar = kind;
					asSelector.integer = literals[i];
					throw CompileError("duplicate case value '" + integerText(asSelector) + "'");
				}
			}
			for (const StatementPtr& child: kase.body) {
				if (child->kind == StatementKind::DeclarationStatement) {
					const auto& declaration = *child->declaration;
					if (i + 1 < statement.switchCases.size() && (declaration.initializer
						|| declaration.type.resource == ResourceKind::Sampler)) {
						throw CompileError("switch label bypasses variable initialization");
					}
					_bindings[declaration.name] = Binding{};
				}
			}
		}
	}
	for (const StatementPtr& child: statement.switchPreamble) {
		if (child->kind == StatementKind::DeclarationStatement) {
			emitVariableDeclaration(*child->declaration, true);
		}
	}
	_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
	std::vector<uint32_t> operands = {
		selector, defaultLabel != InvalidId ? defaultLabel : mergeLabel };
	const uint32_t width = mappingFor(kind).width;
	for (size_t i = 0; i < statement.switchCases.size(); ++i) {
		if (!statement.switchCases[i].value) {
			continue;
		}
		// A literal is as wide as the selector: two words for a 64-bit one.
		operands.push_back(static_cast<uint32_t>(literals[i]));
		if (width > 32) {
			operands.push_back(static_cast<uint32_t>(literals[i] >> 32));
		}
		operands.push_back(labels[i]);
	}
	terminate(spirv::OpSwitch, operands);

	const ControlDepthScope depth(_controlDepth);
	// A break leaves the switch; a continue belongs to the loop around it,
	// which is what a target with no continue label tells the jump lowering.
	_jumpTargets.push_back({ mergeLabel, InvalidId });
	struct PopTarget {
		std::vector<JumpTarget>& targets;
		~PopTarget() { targets.pop_back(); }
	} popTarget{ _jumpTargets };

	for (size_t i = 0; i < statement.switchCases.size(); ++i) {
		beginBlock(labels[i]);
		for (const StatementPtr& child: statement.switchCases[i].body) {
			if (_terminated) {
				_sawDeadCode = true;
				if (_lowerDeadCode) {
					beginBlock(_builder.nextId());
				} else {
					if (child->kind == StatementKind::DeclarationStatement) {
						validateConstexprInitializer(*child->declaration);
						// Only a later case can name it (and then it has no initializer), so storage
						// is emitted only for that; the rest is checked in the scratch pass.
						bool namedLater = false;
						for (size_t later = i + 1; later < statement.switchCases.size() && !namedLater; ++later) {
							for (const StatementPtr& after: statement.switchCases[later].body) {
								namedLater = namedLater || mentionsName(after.get(), child->declaration->name);
							}
						}
						if (namedLater) {
							emitVariableDeclaration(*child->declaration, true);
							if (child->declaration->type.isConstexpr) {
								_localFolded[_bindings[child->declaration->name].id]
									= placeholderConstant(child->declaration->type);
							}
						} else if (child->declaration->type.isConstexpr && !child->declaration->type.isAuto) {
							Binding dead;
							dead.id = _builder.nextId();
							_localFolded[dead.id] = placeholderConstant(child->declaration->type);
							_bindings[child->declaration->name] = dead;
						}
					}
					continue;
				}
			}
			emitStatement(*child);
		}
		branchUnlessTerminated(i + 1 < labels.size() ? labels[i + 1] : mergeLabel);
	}

	beginBlock(mergeLabel);
}

void Emitter::emitStatement(const Statement& statement) {
	if (!statement.refusal.empty()) {
		throw CompileError(statement.refusal);
	}
	switch (statement.kind) {
		case StatementKind::Compound: {
			const BindingScope scope(_bindings);
			// Whatever follows a return in the same block is unreachable,
			// and a block holds nothing after its terminator.
			for (const StatementPtr& child: statement.children) {
				if (_terminated) {
					_sawDeadCode = true;
					if (_lowerDeadCode) {
						// Scratch pass: Apple type-checks what follows a return, so it is
						// lowered in an unreachable block whose module is thrown away.
						beginBlock(_builder.nextId());
					} else {
						if (child->kind == StatementKind::DeclarationStatement) {
							validateConstexprInitializer(*child->declaration);
							// Its name still counts as a constant for the declarations after it.
							if (child->declaration->type.isConstexpr && !child->declaration->type.isAuto) {
								Binding dead;
								dead.id = _builder.nextId();
								_localFolded[dead.id] = placeholderConstant(child->declaration->type);
								_bindings[child->declaration->name] = dead;
							}
						}
						continue;
					}
				}
				emitStatement(*child);
			}
			return;
		}

		case StatementKind::ExpressionStatement:
			if (statement.expression) {
				emitExpressionStatement(*statement.expression);
			}
			return;

		case StatementKind::DeclarationStatement:
			emitVariableDeclaration(*statement.declaration);
			return;

		case StatementKind::Return:
			emitReturn(statement);
			return;

		// Vulkan requires structured control flow: every conditional branch
		// is preceded by a merge instruction naming where its paths rejoin.
		case StatementKind::If: {
			const Id condition = emitCondition(*statement.expression);
			const Id thenLabel = _builder.nextId();
			const Id mergeLabel = _builder.nextId();
			const Id elseLabel = statement.elseBranch ? _builder.nextId() : mergeLabel;

			_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
			terminate(spirv::OpBranchConditional, { condition, thenLabel, elseLabel });

			const ControlDepthScope depth(_controlDepth);
			beginBlock(thenLabel);
			emitStatement(*statement.thenBranch);
			branchUnlessTerminated(mergeLabel);

			if (statement.elseBranch) {
				beginBlock(elseLabel);
				emitStatement(*statement.elseBranch);
				branchUnlessTerminated(mergeLabel);
			}

			beginBlock(mergeLabel);
			return;
		}

		// Loops share one shape: the header evaluates the condition and
		// declares the merge and continue targets, the body branches to the
		// continue block, and that block alone branches back to the header.
		// A do-while moves the condition into the continue block.
		case StatementKind::For:
		case StatementKind::While: {
			const BindingScope scope(_bindings);
			const bool isFor = statement.kind == StatementKind::For;

			if (isFor && statement.forInitializer) {
				emitVariableDeclaration(*statement.forInitializer);
			} else if (isFor && statement.expression) {
				emitExpressionStatement(*statement.expression);
			}

			const Expression* condition = isFor
				? statement.forCondition.get() : statement.whileCondition.get();
			const Id headerLabel = _builder.nextId();
			const Id bodyLabel = _builder.nextId();
			const Id continueLabel = _builder.nextId();
			const Id mergeLabel = _builder.nextId();

			const bool isDoWhile = !isFor && statement.isDoWhile;
			terminate(spirv::OpBranch, { headerLabel });
			beginBlock(headerLabel);
			const ControlDepthScope depth(_controlDepth);
			// The condition is outside the loop body, but nothing in it can
			// be a statement, so the targets may be pushed before it.
			_jumpTargets.push_back({ mergeLabel, continueLabel });
			struct PopTarget {
				std::vector<JumpTarget>& targets;
				~PopTarget() { targets.pop_back(); }
			} popTarget{ _jumpTargets };

			// A &&, || or ?: in the condition splits it over several blocks, and
			// OpLoopMerge has to end the header, so it goes in front of them.
			const bool splitsCondition = condition && !isDoWhile && containsBranchingOperator(*condition);
			Id conditionValue = InvalidId;
			if (condition && !splitsCondition && !isDoWhile) {
				conditionValue = emitCondition(*condition);
			}
			_builder.emit(spirv::OpLoopMerge, { mergeLabel, continueLabel, kLoopControlNone });
			if (splitsCondition) {
				const Id conditionLabel = _builder.nextId();
				terminate(spirv::OpBranch, { conditionLabel });
				beginBlock(conditionLabel);
				conditionValue = emitCondition(*condition);
			}
			if (isDoWhile) {
				terminate(spirv::OpBranch, { bodyLabel });
			} else if (condition) {
				terminate(spirv::OpBranchConditional, { conditionValue, bodyLabel, mergeLabel });
			} else {
				terminate(spirv::OpBranch, { bodyLabel });
			}

			beginBlock(bodyLabel);
			emitStatement(isFor ? *statement.forBody : *statement.whileBody);
			branchUnlessTerminated(continueLabel);

			beginBlock(continueLabel);
			if (isFor && statement.forIncrement) {
				emitExpressionStatement(*statement.forIncrement);
			}
			if (isDoWhile) {
				const Id again = emitCondition(*condition);
				terminate(spirv::OpBranchConditional, { again, headerLabel, mergeLabel });
			} else {
				terminate(spirv::OpBranch, { headerLabel });
			}

			beginBlock(mergeLabel);
			return;
		}

		// Both leave the current block for good, so what follows in the same
		// source block is dropped, as after a return. Inside an if the
		// branch leaves the selection for the loop's own blocks, which
		// structured control flow allows.
		case StatementKind::Break: {
			if (_jumpTargets.empty()) {
				throw CompileError("'break' statement not in loop or switch statement");
			}
			terminate(spirv::OpBranch, { _jumpTargets.back().breakLabel });
			return;
		}

		case StatementKind::Continue: {
			for (auto target = _jumpTargets.rbegin(); target != _jumpTargets.rend(); ++target) {
				if (target->continueLabel != InvalidId) {
					terminate(spirv::OpBranch, { target->continueLabel });
					return;
				}
			}
			throw CompileError("'continue' statement not in loop statement");
		}

		// OpKill ends the block for good, so what follows in the same source
		// block is unreachable, as after a return.
		case StatementKind::Discard:
			// A helper is emitted once for each entry point that calls it, so the
			// stage here is the caller's: OpKill in it drops that caller's fragment,
			// and a helper called from a vertex or kernel function is refused below.
			if (_entryPoint == &_libraryScope) {
				throw StageError("discard_fragment() drops a fragment, which only a fragment function has");
			}
			if (_entryPoint->stage != Stage::Fragment) {
				throw StageError(std::string("discard_fragment() is not allowed within a ")
					+ (_entryPoint->stage == Stage::Vertex ? "vertex" : "kernel")
					+ " function: it drops the fragment, and this function has none");
			}
			terminate(spirv::OpKill, {});
			return;
		case StatementKind::Switch: {
			// A local declared under one label is visible under the ones after
			// it, as C++ says, so the whole body shares one scope.
			const BindingScope scope(_bindings);
			emitSwitch(statement);
			return;
		}
	}
}

void Emitter::emitFunctionBody(const Statement& statement) {
	emitStatement(statement);
}

} // namespace mslc
