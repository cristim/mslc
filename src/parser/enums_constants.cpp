#include "parser/internal.h"

namespace mslc {

std::string Parser::parseEnumDeclaration(bool& hadBody) {
	expectKeyword("enum", "to start an enum");

	if (atKeyword("class") || atKeyword("struct")) {
		throw CompileError("a scoped enum (enum " + std::string(current().text) + ") is not supported; "
			"declare an unscoped enum");
	}

	std::string tag;
	if (kind() == TokenKind::Identifier) {
		tag = std::string(advance().text);
	}

	if (at(TokenKind::Colon)) {
		throw CompileError("an enum with a fixed underlying type is not supported");
	}

	hadBody = at(TokenKind::LBrace);
	if (!tag.empty()) {
		QualifiedName spelled;
		spelled.parts = {tag};
		tag = hadBody ? qualify(tag) : resolveName(spelled);
	}
	if (!hadBody) {
		return tag;
	}

	advance();
	if (!tag.empty()) {
		declareName(tag, "enum");
		_enumTypes[tag] = tag;
		_enumUnderlying[tag] = ScalarKind::Int;
	}

	// Each enumerator is the previous one plus one unless it says otherwise, and
	// the first is zero. They are kept as 64-bit values and held to what an int or
	// an unsigned int holds, so a value that wrapped could never be mistaken for a
	// small one. An unfixed enum is an int unless a value passes INT_MAX, and then
	// every enumerator of it is an unsigned int, the small ones too (Apple:
	// "kSmall - 4 > 0" is true beside a 0xffffffff).
	int64_t next = 0;
	// Until the closing brace an enumerator has the type of its initializer, or the previous enumerator's
	// when it has none ([dcl.enum]/5), so one given an unsigned value is unsigned in the initializers after it.
	ScalarKind nextKind = ScalarKind::Int;
	int64_t lowest = 0;
	int64_t highest = 0;
	std::vector<std::string> names;
	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected an enumerator name, found " + std::string(tokenKindName(kind())));
		}

		const std::string name = qualify(std::string(advance().text));
		int64_t value = next;
		ScalarKind kind = nextKind;
		if (match(TokenKind::Assign)) {
			const ExpressionPtr initializer = parseAssignment();
			value = evaluateConstant(*initializer, false);
			kind = containsUnsigned(*initializer) ? ScalarKind::UInt : ScalarKind::Int;
		}
		if (value > UINT_MAX || value < -INT_MAX) {
			throw CompileError("the enumerator \"" + name + "\" does not fit in an int or an unsigned int");
		}

		declareName(name, "enum constant");
		_enumConstants[name] = {value, kind, tag.empty() ? std::string("<anonymous>") : tag};
		names.push_back(name);
		lowest = std::min(lowest, value);
		highest = std::max(highest, value);
		next = value + 1;
		nextKind = kind;

		if (!match(TokenKind::Comma)) {
			break;
		}
	}

	expect(TokenKind::RBrace, "to close an enum body");

	ScalarKind underlying = ScalarKind::Int;
	if (highest > INT_MAX) {
		if (lowest < 0) {
			throw CompileError("an enum with a negative enumerator and one past INT_MAX needs a 64-bit type, "
				"which mslc has no enum for");
		}
		underlying = ScalarKind::UInt;
	}
	// After it, every enumerator has the enum's type, which promotes to its underlying one.
	for (const std::string& name : names) {
		_enumConstants[name].kind = underlying;
	}
	_lastEnumUnderlying = underlying;
	if (!tag.empty()) {
		_enumUnderlying[tag] = underlying;
	}
	return tag;
}

// Whether an unsigned operand (a sizeof, a u literal, or a named unsigned constant such as an unsigned template
// parameter) is anywhere in a constant expression, which makes the operation around it unsigned ([expr.arith.conv]).
// The evaluator works in signed arithmetic and refuses the operations whose unsigned result would differ.
bool Parser::containsUnsigned(const Expression& expression) const {
	if (expression.kind == ExpressionKind::IntLiteral) {
		return expression.isSizeof || expression.intKind == ScalarKind::UInt || expression.intKind == ScalarKind::ULong;
	}
	if (expression.kind == ExpressionKind::Identifier) {
		const ConstantInt* found = findConstantInt(expression);
		return found && (found->kind == ScalarKind::UInt || found->kind == ScalarKind::ULong);
	}
	if (expression.kind == ExpressionKind::Construct) {
		// A cast to a type no wider than an int gives an int, which wraps nothing.
		const ScalarKind target = expression.constructType ? expression.constructType->scalar : ScalarKind::Int;
		return (target == ScalarKind::UInt || target == ScalarKind::ULong)
			&& expression.arguments.size() == 1 && containsUnsigned(*expression.arguments.front());
	}
	// A conditional's type is that of its two values after the usual arithmetic conversions ([expr.cond]): either
	// one being unsigned makes it unsigned, whichever is chosen. Its condition does not take part.
	if (expression.kind == ExpressionKind::Conditional) {
		return expression.arguments.size() == 2
			&& (containsUnsigned(*expression.arguments[0]) || containsUnsigned(*expression.arguments[1]));
	}
	return (expression.left && containsUnsigned(*expression.left))
		|| (expression.right && containsUnsigned(*expression.right));
}

// The integer constant a name denotes, looked up as namedConstantValue looks it up, or nothing.
const Parser::ConstantInt* Parser::findConstantInt(const Expression& identifier) const {
	if (!identifier.fileScope) {
		for (auto scope = _blocks.rbegin(); scope != _blocks.rend(); ++scope) {
			if (scope->names.count(identifier.name)) {
				const auto constant = scope->constants.find(identifier.name);
				return constant == scope->constants.end() ? nullptr : &constant->second;
			}
		}
	}
	const auto constant = _constantInts.find(identifier.name);
	return constant == _constantInts.end() ? nullptr : &constant->second;
}

// Whether a sizeof is anywhere under an expression, through casts of any type.
static bool mentionsSizeof(const Expression& expression) {
	if (expression.kind == ExpressionKind::IntLiteral) {
		return expression.isSizeof;
	}
	for (const ExpressionPtr& argument: expression.arguments) {
		if (mentionsSizeof(*argument)) {
			return true;
		}
	}
	return (expression.left && mentionsSizeof(*expression.left))
		|| (expression.right && mentionsSizeof(*expression.right));
}

// The range an integer type holds, for a cast of a constant to it. Nothing for a type that is not
// an integer one.
bool integerRange(ScalarKind kind, int64_t& low, int64_t& high) {
	low = 0;
	switch (kind) {
		case ScalarKind::Char: low = INT8_MIN; high = INT8_MAX; return true;
		case ScalarKind::UChar: high = UINT8_MAX; return true;
		case ScalarKind::Short: low = INT16_MIN; high = INT16_MAX; return true;
		case ScalarKind::UShort: high = UINT16_MAX; return true;
		case ScalarKind::Int: low = INT_MIN; high = INT_MAX; return true;
		case ScalarKind::UInt: high = UINT32_MAX; return true;
		case ScalarKind::Long: low = INT64_MIN; high = INT64_MAX; return true;
		case ScalarKind::ULong: high = INT64_MAX; return true;
		default: return false;
	}
}

// Folds the integer constant expressions an enumerator, an attribute index or an
// array length may be. Every step is held to what an int holds, so a result that
// would wrap is an error here, and that is narrower than Apple's compiler, which
// widens. A `u` literal is only accepted as the whole expression, since mixing it
// with a signed operand changes the arithmetic to unsigned, which this does not
// model.
int64_t Parser::evaluateConstant(const Expression& expression, bool nested, bool attribute) const {
	const auto fit = [](int64_t value) {
		if (value > INT_MAX || value < -INT_MAX) {
			throw CompileError("an integer constant expression overflows an int");
		}
		return value;
	};

	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
			if (expression.enumName == "metal::component") {
				throw CompileError("a value of scoped enum metal::component is not an integer constant expression");
			}
			// A sizeof is a ulong, which unsigned arithmetic would wrap but this does not, so
			// the operators below refuse the one case where a signed result differs.
			if (expression.isSizeof) {
				return fit(static_cast<int64_t>(expression.intValue));
			}
			if (nested && expression.intValue > INT_MAX) {
				throw CompileError("an integer above INT_MAX inside a constant expression is not supported");
			}
			if (expression.intValue > INT64_MAX) {
				throw CompileError("an integer constant is too large");
			}
			return static_cast<int64_t>(expression.intValue);
		case ExpressionKind::BoolLiteral:
			// Apple takes a bool literal where an array length or an enumerator is, and refuses one inside an
			// attribute index (a named bool constant is accepted there).
			if (attribute) {
				throw CompileError("a bool is not an index; use an int or a uint");
			}
			return expression.boolValue ? 1 : 0;
		case ExpressionKind::Identifier:
			return namedConstantValue(expression, nested, attribute);
		case ExpressionKind::Index:
			return constantArrayElement(expression, nested, attribute);
		case ExpressionKind::Unary: {
			const int64_t operand = evaluateConstant(*expression.left, true, attribute);
			if ((expression.unaryOperator == UnaryOperator::Negate || expression.unaryOperator == UnaryOperator::BitNot)
				&& containsUnsigned(*expression.left)) {
				throw CompileError("an unsigned operand (a sizeof, a u literal or an unsigned constant), negated or "
					"complemented, wraps, and a constant expression that does so is not supported");
			}
			switch (expression.unaryOperator) {
				case UnaryOperator::Negate: return fit(-operand);
				case UnaryOperator::Plus: return operand;
				case UnaryOperator::Not: return operand == 0 ? 1 : 0;
				case UnaryOperator::BitNot: return ~operand;
				default: break;
			}
			break;
		}
		case ExpressionKind::Binary: {
			const int64_t left = evaluateConstant(*expression.left, true, attribute);

			// The right operand is evaluated only when the left does not decide
			// the result, so `0 && 1 / 0` is a constant and not a division by zero.
			if (expression.binaryOperator == BinaryOperator::LogicalAnd && left == 0) {
				return 0;
			}
			if (expression.binaryOperator == BinaryOperator::LogicalOr && left != 0) {
				return 1;
			}

			const int64_t right = evaluateConstant(*expression.right, true, attribute);
			// An unsigned operand makes the operation unsigned, so a negative operand converts to a huge one
			// and a negative result wraps; neither is modelled, and neither is guessed at. Otherwise the
			// signed result of every operator here is the unsigned one.
			if ((containsUnsigned(*expression.left) || containsUnsigned(*expression.right))
				&& (left < 0 || right < 0 || (expression.binaryOperator == BinaryOperator::Subtract && left < right))) {
				throw CompileError("a constant expression that mixes an unsigned operand with a negative value or "
					"subtracts below zero would wrap as an unsigned one, which is not supported");
			}
			switch (expression.binaryOperator) {
				case BinaryOperator::Add: return fit(left + right);
				case BinaryOperator::Subtract: return fit(left - right);
				case BinaryOperator::Multiply: return fit(left * right);
				case BinaryOperator::Divide:
				case BinaryOperator::Modulo:
					if (right == 0) {
						throw CompileError("division by zero in a constant expression");
					}
					return expression.binaryOperator == BinaryOperator::Divide ? fit(left / right) : left % right;
				case BinaryOperator::ShiftLeft:
				case BinaryOperator::ShiftRight:
					if (right < 0 || right > 31 || left < 0) {
						throw CompileError("a shift in a constant expression has a negative operand or a "
							"count outside 0 to 31");
					}
					return expression.binaryOperator == BinaryOperator::ShiftLeft ? fit(left << right) : left >> right;
				case BinaryOperator::BitAnd: return left & right;
				case BinaryOperator::BitOr: return left | right;
				case BinaryOperator::BitXor: return left ^ right;
				case BinaryOperator::Less: return left < right;
				case BinaryOperator::LessEqual: return left <= right;
				case BinaryOperator::Greater: return left > right;
				case BinaryOperator::GreaterEqual: return left >= right;
				case BinaryOperator::Equal: return left == right;
				case BinaryOperator::NotEqual: return left != right;
				case BinaryOperator::LogicalAnd: return left != 0 && right != 0;
				case BinaryOperator::LogicalOr: return left != 0 || right != 0;
			}
			break;
		}
		case ExpressionKind::Conditional: {
			// Only the value the condition chooses is evaluated ([expr.cond]), so the other may be one no constant
			// expression holds, such as a division by zero.
			if (expression.arguments.size() != 2) {
				break;
			}
			// The condition is a bool, not the index, so a bool literal is fine there in an attribute index.
			const int64_t condition = evaluateConstant(*expression.left, true, false);
			const int64_t value = evaluateConstant(*expression.arguments[condition != 0 ? 0 : 1], nested, attribute);
			// An unsigned value on either side makes the conditional unsigned, so a negative one chosen converts
			// to a huge one, which is not modelled.
			if (value < 0 && containsUnsigned(expression)) {
				throw CompileError("a constant expression that mixes an unsigned operand with a negative value or "
					"subtracts below zero would wrap as an unsigned one, which is not supported");
			}
			return value;
		}
		case ExpressionKind::Sizeof:
			throw CompileError("the size of an expression is worked out after parsing, so it is not an "
				"array length or an attribute index; write sizeof(<type>)");
		case ExpressionKind::Construct: {
			// A cast is only taken of a sizeof, which is the one ulong an attribute index or an array
			// length is written from; (uint)sizeof(S) and int(sizeof(S)) hold the same value.
			int64_t low = 0;
			int64_t high = 0;
			if (expression.arguments.size() != 1 || !mentionsSizeof(*expression.arguments.front())
				|| !expression.constructType || expression.constructType->isPointer
				|| expression.constructType->arrayLength || !expression.constructType->isScalar()
				|| !expression.constructType->namedType.empty() || !expression.constructType->enumName.empty()
				|| !integerRange(expression.constructType->scalar, low, high)) {
				break;
			}
			const int64_t operand = evaluateConstant(*expression.arguments.front(), nested, attribute);
			if (operand < low || operand > high) {
				throw CompileError("a cast of a constant to a type that cannot hold it wraps, which is "
					"not supported");
			}
			return operand;
		}
		default:
			break;
	}

	throw CompileError("not an integer constant expression");
}

// A name in a constant expression: an enumerator the parser already replaced with its value, or an
// integer declared const or constexpr with an initialiser that folded. The innermost block that
// declares the name decides, so a local that hides a constant is not one. Only an int-sized type
// takes part in arithmetic: an unsigned or wider one would need unsigned arithmetic, which
// evaluateConstant does not model, so it is accepted as the whole expression and nothing else.
int64_t Parser::namedConstantValue(const Expression& identifier, bool nested, bool attribute) const {
	const std::string& name = identifier.name;
	const ConstantInt* found = nullptr;
	std::string global = name;
	// Inside a struct body a member declared above hides a file-scope constant of its name, and a
	// non-static member is not a constant (Apple: invalid use of non-static data member).
	if (_structBody && !identifier.fileScope) {
		for (const StructField& field: _structBody->fields) {
			if (field.name == name) {
				throw CompileError("\"" + name + "\" is a non-static data member, not a constant");
			}
		}
	}
	if (!identifier.fileScope) {
		for (auto scope = _blocks.rbegin(); scope != _blocks.rend(); ++scope) {
			const auto entry = scope->names.find(name);
			if (entry == scope->names.end()) {
				continue;
			}
			const auto constant = scope->constants.find(name);
			if (constant == scope->constants.end()) {
				throw CompileError("\"" + name + "\" is a variable, not a constant: only a const or constexpr "
					"integer with a constant initialiser is");
			}
			found = &constant->second;
			break;
		}
	}
	if (!found) {
		const auto constant = _constantInts.find(global);
		if (constant == _constantInts.end()) {
			throw CompileError("\"" + name + "\" is not an integer constant this source declares before this "
				"one (a const or constexpr integer with a constant initialiser is)");
		}
		found = &constant->second;
	}

	if (attribute && (found->kind == ScalarKind::Long || found->kind == ScalarKind::ULong)) {
		throw CompileError("a long is not an index; use an int or a uint");
	}
	if (nested && (found->value > INT_MAX || found->value < -INT_MAX)) {
		throw CompileError("an integer constant outside what an int holds inside a constant expression is not "
			"supported");
	}
	return found->value;
}

// An element of a constant integer array, `table[2]`: the array is a const or constexpr one-dimensional
// array of integers with a brace initialiser that folded (recordConstantArray), and the index is itself
// a constant expression inside the array's bounds. The name is looked up the way namedConstantValue
// looks up an integer constant, so a local or a member that hides the array is not one.
int64_t Parser::constantArrayElement(const Expression& element, bool nested, bool attribute) const {
	if (element.isDereference || !element.left || element.left->kind != ExpressionKind::Identifier
		|| element.arguments.size() != 1) {
		throw CompileError("not an integer constant expression");
	}
	const Expression& identifier = *element.left;
	const std::string& name = identifier.name;
	const ConstantIntArray* found = nullptr;
	if (_structBody && !identifier.fileScope) {
		for (const StructField& field: _structBody->fields) {
			if (field.name == name) {
				throw CompileError("\"" + name + "\" is a non-static data member, not a constant");
			}
		}
	}
	if (!identifier.fileScope) {
		for (auto scope = _blocks.rbegin(); scope != _blocks.rend(); ++scope) {
			if (!scope->names.count(name)) {
				continue;
			}
			const auto array = scope->constantArrays.find(name);
			if (array == scope->constantArrays.end()) {
				throw CompileError("\"" + name + "\" is not a constant integer array: only a const or constexpr "
					"array of integers with a constant brace initialiser is");
			}
			found = &array->second;
			break;
		}
	}
	if (!found) {
		const auto array = _constantIntArrays.find(name);
		if (array == _constantIntArrays.end()) {
			throw CompileError("\"" + name + "\" is not a constant integer array this source declares before "
				"this one (a const or constexpr array of integers with a constant brace initialiser is)");
		}
		found = &array->second;
	}

	const int64_t index = evaluateConstant(*element.arguments.front(), true, false);
	if (index < 0 || static_cast<uint64_t>(index) >= found->values.size()) {
		throw CompileError("the index " + std::to_string(index) + " is outside the array \"" + name
			+ "\" of " + std::to_string(found->values.size()) + " elements");
	}
	if (attribute && (found->kind == ScalarKind::Long || found->kind == ScalarKind::ULong)) {
		throw CompileError("a long is not an index; use an int or a uint");
	}
	// An element's unsignedness is not tracked (containsUnsigned does not look into arrays), so an unsigned or
	// wider array is still taken only as the whole expression.
	const bool intSized = found->kind != ScalarKind::UInt && found->kind != ScalarKind::Long
		&& found->kind != ScalarKind::ULong;
	if (nested && !intSized) {
		throw CompileError("an element of an unsigned or wider array inside a constant expression is not supported");
	}
	return found->values[static_cast<size_t>(index)];
}

// A const or constexpr array of integers, one dimension, initialised by a brace list whose elements all
// fold: the elements are kept so a constant expression can index the array. Elements the list leaves out
// are zero, as C++ says. Anything else (a designator, a nested list, a value its type cannot hold, a list
// longer than the array) leaves the array unrecorded, so indexing it in a constant expression is refused.
void Parser::recordConstantArray(const VariableDeclaration& declaration, bool local) {
	const Type& type = declaration.type;
	if (!(type.isConst || type.isConstexpr || !local) || type.isPointer || type.arrayRank() != 1
		|| type.vectorWidth != 0 || !declaration.initializer
		|| declaration.initializer->kind != ExpressionKind::InitList) {
		return;
	}
	int64_t low = 0;
	int64_t high = 0;
	if (!integerRange(type.scalar, low, high)) {
		return;
	}
	const std::vector<InitializerElement>& elements = declaration.initializer->elements;
	if (elements.size() > *type.arrayLength) {
		return;
	}
	ConstantIntArray array;
	array.kind = type.scalar;
	array.values.assign(*type.arrayLength, 0);
	for (size_t i = 0; i < elements.size(); ++i) {
		if (!elements[i].fieldName.empty() || !elements[i].value) {
			return;
		}
		int64_t value = 0;
		try {
			value = evaluateConstant(*elements[i].value, false);
		} catch (const CompileError&) {
			return;
		}
		if (value < low || value > high) {
			return;
		}
		array.values[i] = value;
	}
	if (local) {
		_blocks.back().constantArrays[declaration.name] = std::move(array);
	} else {
		_constantIntArrays[declaration.name] = std::move(array);
	}
}

// An integer declared const or constexpr (or at file scope) with a constant initialiser may be
// named by a later array length; a value its own type cannot hold is left out.
void Parser::recordConstant(const VariableDeclaration& declaration, bool local) {
	const Type& type = declaration.type;
	if (type.arrayLength) {
		recordConstantArray(declaration, local);
		return;
	}
	if (!(type.isConst || type.isConstexpr || !local) || type.isPointer || (!local && type.isAtomic)
		|| !type.isScalar() || !type.namedType.empty() || type.enumName == "metal::component"
		|| !declaration.initializer) {
		return;
	}
	int64_t low = 0;
	int64_t high = 0;
	switch (type.scalar) {
		case ScalarKind::Bool: low = 0; high = 1; break;
		case ScalarKind::Char: low = INT8_MIN; high = INT8_MAX; break;
		case ScalarKind::UChar: high = UINT8_MAX; break;
		case ScalarKind::Short: low = INT16_MIN; high = INT16_MAX; break;
		case ScalarKind::UShort: high = UINT16_MAX; break;
		case ScalarKind::Int: low = INT32_MIN; high = INT32_MAX; break;
		case ScalarKind::UInt: high = UINT32_MAX; break;
		case ScalarKind::Long: low = INT64_MIN; high = INT64_MAX; break;
		case ScalarKind::ULong: high = INT64_MAX; break;
		default: return;
	}

	const Expression* initializer = declaration.initializer.get();
	while (initializer->kind == ExpressionKind::InitList && !initializer->constructType
		&& initializer->elements.size() == 1 && initializer->elements.front().fieldName.empty()) {
		initializer = initializer->elements.front().value.get();
	}
	if (initializer->kind == ExpressionKind::Construct && initializer->arguments.size() == 1
		&& initializer->constructType && initializer->constructType->scalar == type.scalar
		&& initializer->constructType->isScalar() && !initializer->constructType->isPointer) {
		initializer = initializer->arguments.front().get();
	}
	int64_t value = 0;
	if (!(initializer->kind == ExpressionKind::InitList && !initializer->constructType
		&& initializer->elements.empty())) {
		try {
			value = evaluateConstant(*initializer, false);
		} catch (const CompileError&) {
			return;
		}
	}
	if (type.scalar == ScalarKind::Bool && value != 0) {
		value = 1;
	}
	if (value < low || value > high) {
		return;
	}

	if (local) {
		_blocks.back().constants[declaration.name] = {value, type.scalar};
	} else {
		_constantInts[declaration.name] = {value, type.scalar};
	}
}

// A local or a parameter keeps its type for sizeof once the declarator is complete. An auto, a
// resource or an array with no length yet is left out, so a sizeof of it is not folded here.
void Parser::recordObjectType(const std::string& name, const Type& type) {
	if (name.empty() || type.isAuto || type.resource != ResourceKind::None || _blocks.empty()) {
		return;
	}
	_blocks.back().objectTypes[name] = type;
}

// The declared type of a name or of an element of one, as the innermost declaration that the name
// finds says. Nothing else is typed here (a member, a call, a vector component), and a name whose
// innermost declaration has no recorded type is not looked up further out, which would size a
// variable that hides it wrongly.
std::optional<Type> Parser::declaredObjectType(const Expression& operand) const {
	if (operand.kind == ExpressionKind::Index) {
		std::optional<Type> base = declaredObjectType(*operand.left);
		if (!base || (base->isPointer && base->arrayLength)) {
			return std::nullopt;
		}
		if (base->arrayLength) {
			base->dropOuterDimension();
		} else if (base->isPointer) {
			// A decayed "float m[2][3]" points at arrays of 3, whose lengths stay in the type.
			base->isPointer = false;
			base->dropOuterDimension();
		} else {
			return std::nullopt;
		}
		return base;
	}
	if (operand.kind != ExpressionKind::Identifier || operand.templateDependent) {
		return std::nullopt;
	}
	if (_structBody) {
		for (const StructField& field: _structBody->fields) {
			if (field.name == operand.name) {
				return std::nullopt;
			}
		}
	}
	if (!operand.fileScope) {
		for (auto scope = _blocks.rbegin(); scope != _blocks.rend(); ++scope) {
			if (!scope->names.count(operand.name)) {
				continue;
			}
			const auto type = scope->objectTypes.find(operand.name);
			return type == scope->objectTypes.end() ? std::nullopt : std::optional<Type>(type->second);
		}
	}
	for (const VariableDeclaration& global: _unit.globals) {
		if (global.name == operand.name) {
			const Type& type = global.type;
			if (type.isAuto || type.resource != ResourceKind::None) {
				return std::nullopt;
			}
			return type;
		}
	}
	return std::nullopt;
}

// Whether a constant expression has a long or ulong in it, which makes its
// type one. A shift has the type of its left operand alone.
static bool isSixtyFourBit(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
			if (expression.enumName == "metal::component") {
				throw CompileError("a value of scoped enum metal::component is not an integer constant expression");
			}
			return expression.intKind == ScalarKind::Long || expression.intKind == ScalarKind::ULong;
		case ExpressionKind::Unary:
			return isSixtyFourBit(*expression.left);
		case ExpressionKind::Construct:
			if (expression.constructType && expression.constructType->isScalar() && !expression.constructType->isPointer) {
				return expression.constructType->scalar == ScalarKind::Long
					|| expression.constructType->scalar == ScalarKind::ULong;
			}
			return false;
		case ExpressionKind::Binary: {
			const bool isShift = expression.binaryOperator == BinaryOperator::ShiftLeft
				|| expression.binaryOperator == BinaryOperator::ShiftRight;
			return isSixtyFourBit(*expression.left) || (!isShift && isSixtyFourBit(*expression.right));
		}
		default:
			return false;
	}
}

// The index an attribute or an array length takes: a constant that is not
// negative and fits the 32 bits it is stored in. Apple takes an int or a uint
// for an attribute and rejects a long, though an array length may be one.
int64_t Parser::parseConstantIndex(const std::string& context, bool isAttribute) {
	int64_t value = 0;
	try {
		const ExpressionPtr expression = parseAssignment();
		if (isAttribute && isSixtyFourBit(*expression)) {
			throw CompileError("a long is not an index; use an int or a uint");
		}
		value = evaluateConstant(*expression, false, isAttribute);
	} catch (const CompileError& error) {
		throw CompileError(context + " needs a constant integer argument (" + error.what() + ")");
	}

	if (value < 0 || value > UINT32_MAX) {
		throw CompileError(context + " needs a constant integer argument between 0 and 4294967295, found "
			+ std::to_string(value));
	}

	return value;
}

} // namespace mslc
