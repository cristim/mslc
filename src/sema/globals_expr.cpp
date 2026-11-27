#include "sema/emitter.h"

namespace mslc {

bool sameArrayType(const Type& expected, const Type& got) {
	return expected.isStdArray && got.isStdArray
		&& expected.arrayLength == got.arrayLength
		&& expected.innerArrayLengths == got.innerArrayLengths
		&& expected.scalar == got.scalar
		&& expected.vectorWidth == got.vectorWidth
		&& expected.matrixColumns == got.matrixColumns
		&& expected.isPacked == got.isPacked
		&& expected.namedType == got.namedType
		&& expected.enumName == got.enumName
		&& expected.isPointer == got.isPointer
		&& expected.isAtomic == got.isAtomic
		&& expected.resource == got.resource;
}

void requireArrayElementMatch(const Type& expected, const Expression& expression) {
	if (expression.kind != ExpressionKind::InitList || !expression.constructType) {
		return;
	}
	const Type& got = *expression.constructType;
	const bool match = expected.arrayLength
		? sameArrayType(expected, got)
		: (!got.arrayLength && got.namedType == expected.namedType);
	if (!match) {
		throw CompileError("cannot initialize \"" + typeName(expected) + "\" from \""
			+ typeName(got) + "\"");
	}
}

// A file-scope constant array is a Private variable with a constant initialiser, which
// no store reaches. Vulkan has no way to index a constant composite by a run-time index,
// so the alternative is a Function copy at every use; one module-scope variable is read
// in place by a kernel and a helper alike, and indexing it is one access chain.
void Emitter::declareFileScopeArray(const VariableDeclaration& declaration, size_t position) {
	const Type& type = declaration.type;
	if (type.isPointer || type.isAtomic || type.resource != ResourceKind::None) {
		throw CompileError("a file-scope array of pointers, atomics or resources is not lowered yet");
	}
	if (!declaration.initializer || declaration.initializer->kind != ExpressionKind::InitList
		|| (declaration.initializer->constructType && !type.isStdArray)) {
		throw CompileError("array initializer must be an initializer list");
	}
	requireArrayElementMatch(type, *declaration.initializer);

	ArrayLeaves leaves;
	leaves.element = [&](const Type& leaf, const Expression* entry) {
		if (!entry) {
			return _types.zero(declaredTypeOf(leaf));
		}
		if (!leaf.enumName.empty()) {
			requireEnumValue(leaf.enumName, *entry, "an element of the initializer list");
		}
		_foldDomainError = false;
		const FoldedConstant folded = foldInitializer(leaf, *entry, true);
		if (_foldDomainError && type.isConstexpr) {
			throw CompileError("constexpr variable \"" + declaration.name + "\" must be initialized by a "
				"constant expression");
		}
		_foldDomainError = false;
		return emitConstant(folded);
	};
	leaves.compose = [&](Id composite, const std::vector<uint32_t>& parts) {
		return constantStruct(composite, parts);
	};
	size_t consumed = 0;
	const Id initial = emitArrayInit(type, declaration.initializer->elements, consumed, true, leaves);

	FileScopeArray array;
	array.name = declaration.name;
	array.position = position;
	array.type = _builder.typeOf(initial);
	array.msl = type;
	array.variable = _builder.emitDeclTyped(spirv::OpVariable,
		_types.pointer(spirv::StorageClass::Private, array.type),
		{ static_cast<uint32_t>(spirv::StorageClass::Private), initial });
	_fileScopeArrays.push_back(array);
}

// The arrays a function can see are the ones declared above it. A parameter or local
// of the same name is bound afterwards and wins.
void Emitter::bindFileScopeArrays(size_t declaredBefore) {
	_fileScopeBound.clear();
	for (const FileScopeArray& array: _fileScopeArrays) {
		if (array.position >= declaredBefore) {
			continue;
		}
		Binding binding;
		binding.id = array.variable;
		binding.isPointer = true;
		binding.pointeeType = array.type;
		binding.storageClass = spirv::StorageClass::Private;
		binding.pointeeMsl = array.msl;
		binding.readOnly = true;
		_bindings[array.name] = binding;
		_fileScopeBound[array.name] = binding;
	}
}

// "::name" names what the file declares, so the entry that a local of that name
// holds is set aside for the expression; see FileScopeView.
// Evaluating anything else while any is in force suspends them all.
std::unique_ptr<FileScopeView> Emitter::viewFileScope(const Expression& expression) {
	const Expression* root = &expression;
	while ((root->kind == ExpressionKind::Member || root->kind == ExpressionKind::Index) && root->left) {
		root = root->left.get();
	}
	const auto hidden = [&](const std::string& name) {
		return std::any_of(_fileScopeViews.begin(), _fileScopeViews.end(),
			[&](const FileScopeView* view) { return view->hides(name); });
	};
	if (root->kind == ExpressionKind::Identifier && root->fileScope) {
		if (hidden(root->name)) {
			return std::make_unique<FileScopeView>(_fileScopeViews);
		}
		const auto array = _fileScopeBound.find(root->name);
		return std::make_unique<FileScopeView>(_bindings, _fileScopeViews, root->name,
			array == _fileScopeBound.end() ? nullptr : &array->second);
	}
	const bool anyHidden = std::any_of(_fileScopeViews.begin(), _fileScopeViews.end(),
		[](const FileScopeView* view) { return view->hiding(); });
	return std::make_unique<FileScopeView>(_fileScopeViews, anyHidden);
}

// Whether evaluating an expression again only reads: names, literals, members, elements and arithmetic on them.
static bool readsWithoutEffect(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
		case ExpressionKind::FloatLiteral:
		case ExpressionKind::BoolLiteral:
		case ExpressionKind::Identifier: return true;
		case ExpressionKind::Member: return expression.left && readsWithoutEffect(*expression.left);
		case ExpressionKind::Index:
			return expression.left && readsWithoutEffect(*expression.left) && std::all_of(expression.arguments.begin(),
				expression.arguments.end(), [](const ExpressionPtr& index) { return index && readsWithoutEffect(*index); });
		case ExpressionKind::Binary:
			return expression.left && expression.right && readsWithoutEffect(*expression.left)
				&& readsWithoutEffect(*expression.right);
		case ExpressionKind::Unary:
			return (expression.unaryOperator == UnaryOperator::Negate || expression.unaryOperator == UnaryOperator::Plus
				|| expression.unaryOperator == UnaryOperator::Not || expression.unaryOperator == UnaryOperator::BitNot)
				&& expression.left && readsWithoutEffect(*expression.left);
		default: return false;
	}
}

// Whether an assignment's left operand names an element: then its index may change with the store, and the
// value is loaded from the address stored to rather than read again.
static bool hasIndex(const Expression& left) {
	for (const Expression* part = &left; part; part = part->left.get()) {
		if (part->kind == ExpressionKind::Index) {
			return true;
		}
		if (part->kind != ExpressionKind::Member) {
			return false;
		}
	}
	return false;
}

Id Emitter::emitExpression(const Expression& expression) {
	const auto fileScopeView = viewFileScope(expression);
	switch (expression.kind) {
		case ExpressionKind::IntLiteral: {
			// The literal's own type, which the parser took from its spelling:
			// "-1" is a negated int and "3000000000" a long, so neither wraps.
			// Emitting every literal as %uint made "-1" OpSNegate %uint, which
			// is 4294967295 as a float3 component, and as %int made
			// "4294967295u / 3u" an OpSDiv, because emitBinary takes the opcode
			// from the left operand's type.
			const Id type = _types.scalar(expression.intKind);
			std::vector<uint32_t> words { static_cast<uint32_t>(expression.intValue) };
			if (scalarBitWidth(expression.intKind) == 64) {
				words.push_back(static_cast<uint32_t>(expression.intValue >> 32));
			}
			return _builder.emitDeclTyped(spirv::OpConstant, type, words);
		}

		case ExpressionKind::FloatLiteral: {
			if (expression.floatIsHalf) {
				return _builder.emitDeclTyped(spirv::OpConstant, _types.scalar(ScalarKind::Half),
					{ halfBitsOf(expression.floatValue) });
			}

			const auto value = static_cast<float>(expression.floatValue);
			uint32_t bits = 0;
			static_assert(sizeof(bits) == sizeof(value), "float is not 32 bits");
			std::memcpy(&bits, &value, sizeof(bits));
			return _builder.emitDeclTyped(spirv::OpConstant,
				_types.scalar(ScalarKind::Float), { bits });
		}

		case ExpressionKind::BoolLiteral: {
			return expression.boolValue
				? _builder.emitDeclTyped(spirv::OpConstantTrue, _boolType, { })
				: _builder.emitDeclTyped(spirv::OpConstantFalse, _boolType, { });
		}

		case ExpressionKind::Identifier: return emitIdentifier(expression);
		case ExpressionKind::Binary: return emitBinary(expression);
		case ExpressionKind::Unary: return emitUnary(expression);
		case ExpressionKind::Index: return emitIndex(expression, false);
		case ExpressionKind::Member: return emitMember(expression);
		case ExpressionKind::Call: return emitCall(expression);
		case ExpressionKind::Construct: return emitConstruct(expression);
		case ExpressionKind::Conditional: return emitConditional(expression);
		case ExpressionKind::Comma:
			emitExpressionStatement(*expression.left);
			return emitExpression(*expression.right);
		case ExpressionKind::Sizeof: {
			const uint64_t size = sizeofOperand(*expression.left);
			return _builder.emitDeclTyped(spirv::OpConstant, _types.scalar(ScalarKind::ULong),
				{ static_cast<uint32_t>(size), static_cast<uint32_t>(size >> 32) });
		}
		case ExpressionKind::InitList:
			// S{...} names its struct; a bare list does not and is only an initialiser.
			if (expression.constructType) {
				return emitInitListValue(*expression.constructType, expression);
			}
			break;
		case ExpressionKind::Assign:
			// The value of an assignment is its left operand once stored to ([expr.ass]), read again here, which
			// gives that value only while reading the operand again does nothing else.
			if (!readsWithoutEffect(*expression.left)) {
				throw CompileError("an assignment whose left operand has a call, an increment or an assignment "
					"in it cannot be used as a value yet; assign first, then use the variable");
			}
			// An element, as a[a[0]] or a[k] with k changed on the right: its address, taken where the store
			// takes it, is the place stored to.
			if (hasIndex(*expression.left)) {
				if (expression.left->kind == ExpressionKind::Member && !structOf(*expression.left->left)) {
					throw CompileError("an assignment to a swizzle of an element cannot be used as a value yet; "
						"assign first, then use the element");
				}
				const Id address = emitPlaceAddress(*expression.left);
				emitAssignment(*expression.left, *expression.right, expression.compoundOperator);
				const Id pointee = valueTypeAt(address);
				const auto storage = _types.storageClassOf(_builder.typeOf(address));
				return storage == spirv::StorageClass::PhysicalStorageBuffer
					? loadFromBuffer(address, pointee) : loadFrom(address, pointee);
			}
			emitAssignment(*expression.left, *expression.right, expression.compoundOperator);
			return emitExpression(*expression.left);
	}

	// A braced list without a type is only spelled in an initialiser, so reaching one here means the source
	// used it as a value, which MSL does not allow.
	throw CompileError("this expression cannot be used as a value");
}

} // namespace mslc
