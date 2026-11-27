#include "sema/emitter.h"

namespace mslc {

Id Emitter::emitUnary(const Expression& expression) {
	if (expression.unaryOperator == UnaryOperator::AddressOf) {
		throw CompileError("taking an address with '&' is not supported; only an atomic_*_explicit "
			"function takes the address of an element");
	}
	if (isComponentValue(*expression.left)) {
		throw CompileError("unary operator does not accept a value of scoped enum metal::component without a cast");
	}
	const Id operand = emitExpression(*expression.left);
	const Id type = _builder.typeOf(operand);

	// Apple rejects every unary operator on a matrix, unary plus included, and
	// OpFNegate takes no matrix.
	if (_types.matrixInfo(type)) {
		throw CompileError("a unary operator on a matrix is not lowered yet");
	}

	TypeTable::StructForm form;
	if (_types.structForm(type, form)) {
		throw CompileError("a unary operator on the struct " + form.name + " is not lowered");
	}

	// ~ on a bool vector is the componentwise complement, a bool vector (MSL 3.1).
	if (_types.isBool(type) && _types.vectorWidth(type) > 1 && expression.unaryOperator == UnaryOperator::BitNot) {
		return _builder.emitTyped(spirv::OpLogicalNot, type, { operand });
	}
	if (_types.isBool(type) && _types.vectorWidth(type) > 1 && expression.unaryOperator != UnaryOperator::Not) {
		throw CompileError("a unary operator other than ! and ~ on a bool vector is not lowered yet");
	}
	// A scalar bool is promoted to int ([conv.prom]): -true is the int -1 and ~true the int -2.
	if (_types.isBool(type) && _types.vectorWidth(type) == 1 && expression.unaryOperator != UnaryOperator::Not) {
		const Id intType = _types.scalar(ScalarKind::Int);
		return emitUnaryOperation(expression.unaryOperator, convert(operand, type, intType), intType);
	}

	// The integer promotion: + - ~ on a scalar narrower than int give an int (! gives a bool; a
	// vector keeps its type, as Apple does), so an overload sees an int however the value is used.
	const bool promotes = expression.unaryOperator != UnaryOperator::Not && !_types.isFloat(type)
		&& _types.vectorWidth(type) == 1 && _types.bitWidth(type) < 32;
	if (promotes) {
		const Id promoted = promotedTo(1);
		return emitUnaryOperation(expression.unaryOperator, convert(operand, type, promoted), promoted);
	}
	return emitUnaryOperation(expression.unaryOperator, operand, type);
}

Id Emitter::emitUnaryOperation(UnaryOperator op, Id operand, Id type) {
	switch (op) {
		case UnaryOperator::Plus: return operand;
		case UnaryOperator::Negate:
			return _types.isFloat(type)
				? _builder.emitTyped(spirv::OpFNegate, type, { operand })
				: _builder.emitTyped(spirv::OpSNegate, type, { operand });
		case UnaryOperator::Not:
			// A bool vector is negated componentwise, and stays one.
			return _types.isBool(type)
				? _builder.emitTyped(spirv::OpLogicalNot, type, { operand })
				: _builder.emitTyped(spirv::OpLogicalNot, _boolType, { asCondition(operand) });
		case UnaryOperator::BitNot:
			if (_types.isFloat(type)) { throw CompileError("operator ~ needs an integer operand"); }
			return _builder.emitTyped(spirv::OpNot, type, { operand });
		default:
			throw CompileError("++ and -- are lowered only as a statement or a for-loop "
				"increment, not for their value");
	}
}

Emitter::ArithmeticConversion Emitter::usualArithmeticConversion(Id leftOperandType, Id rightOperandType) {
	// The two operands have to share a type, and the rule is C's usual
	// arithmetic conversions: a float beats an integer, then the wider
	// integer beats the narrower, and only then does signedness break a tie
	// with both becoming unsigned. Converting one side to the other's type
	// instead narrows: "3 < f" would compare as ints and "b < v" for a uchar
	// and an int would compare as uchars.
	Id leftType = leftOperandType;
	Id rightType = rightOperandType;

	// The integer promotions come before the usual arithmetic conversions,
	// not after them, and skipping them loses a sign. Anything narrower than
	// int becomes int whatever its signedness, so "char a = -1; ushort b = 0;
	// a < b" compares -1 with 0 as two ints and is true. Going straight to the
	// wider type instead widens -1 to 65535, which reads as false.
	//
	// It is only the narrower-than-int types that are wrong, because they are
	// the only ones that fit in an int and so the only ones where the
	// promotion changes the answer. A uint beside an int needs no promotion:
	// the rank rule below leaves the wider type alone.
	//
	// Each operand is promoted on its own, and only the *types* are changed
	// here: the conversions are emitted below, once, when both operands go to
	// the common type. Promoting both sides to one int here instead of each
	// on its own would replace the conversions rather than precede them, and
	// then "char c = -1; uint u = 0; c < u" would compare two ints and answer
	// true where MSL makes both sides unsigned and answers false. It would
	// also truncate a 64-bit operand, which the conversions never do.
	if (!_types.isFloat(leftType) && !_types.isFloat(rightType)) {
		if (_types.bitWidth(leftType) < 32) {
			leftType = promotedTo(_types.vectorWidth(leftType));
		}
		if (_types.bitWidth(rightType) < 32) {
			rightType = promotedTo(_types.vectorWidth(rightType));
		}
	}

	const bool leftIsFloat = _types.isFloat(leftType);
	const bool rightIsFloat = _types.isFloat(rightType);
	// Not named `signed`, which is a keyword.
	bool operandsSigned = _types.isSignedInt(leftType);

	// The type both operands end up in, which is also what the opcode is
	// chosen from. Float wins over integer; then the wider integer; then the
	// left one's own type.
	Id commonType = leftType;
	if (leftIsFloat != rightIsFloat) {
		commonType = leftIsFloat ? leftType : rightType;
		operandsSigned = false;
	} else if (_types.bitWidth(rightType) > _types.bitWidth(leftType)) {
		// The wider type wins outright, signedness and all. A long beside a
		// uint is a signed comparison, because long has the higher rank and can
		// represent every uint; making both sides unsigned here is what turned
		// that pair wrong.
		commonType = rightType;
		operandsSigned = _types.isSignedInt(rightType);
	} else if (_types.bitWidth(leftType) > _types.bitWidth(rightType)) {
		// The left is already the wider, which the default above already says.
	} else if (leftType != rightType) {
		// Equal widths and opposite signedness, which is the only case signedness
		// decides: C makes both sides unsigned.
		operandsSigned = _types.isSignedInt(leftType) && _types.isSignedInt(rightType);
	}

	if (_types.isFloat(commonType)) {
		operandsSigned = false;
	} else if (_types.isSignedInt(commonType) != operandsSigned) {
		// Same width, opposite signedness: the signed side becomes the
		// unsigned kind of the same width, which is what makes "-1 < u" an
		// unsigned comparison of 4294967295 against the value. It is taken from
		// the width, so a 64-bit operand stays 64 bits.
		uint32_t width = _types.bitWidth(commonType);
		if (width == 64) {
			commonType = _types.scalar(ScalarKind::ULong);
		} else if (width == 16) {
			commonType = _types.scalar(ScalarKind::UShort);
		} else if (width == 8) {
			commonType = _types.scalar(ScalarKind::UChar);
		} else {
			commonType = _types.scalar(ScalarKind::UInt);
		}
	}

	return { commonType, operandsSigned };
}


Id Emitter::emitShortCircuit(BinaryOperator op, Id left, const Expression& rightExpression) {
	const bool isAnd = op == BinaryOperator::LogicalAnd;
	const Id condition = asCondition(left);
	const Id leftBlock = _currentBlock;
	const Id rightLabel = _builder.nextId();
	const Id mergeLabel = _builder.nextId();

	_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
	terminate(spirv::OpBranchConditional, { condition,
		isAnd ? rightLabel : mergeLabel, isAnd ? mergeLabel : rightLabel });

	beginBlock(rightLabel);
	const ControlDepthScope depth(_controlDepth);
	const Id right = emitCondition(rightExpression);
	const Id rightBlock = _currentBlock;
	terminate(spirv::OpBranch, { mergeLabel });

	beginBlock(mergeLabel);
	const Id decided = isAnd
		? _builder.emitDeclTyped(spirv::OpConstantFalse, _boolType, { })
		: _builder.emitDeclTyped(spirv::OpConstantTrue, _boolType, { });
	return _builder.emitTyped(spirv::OpPhi, _boolType, { decided, leftBlock, right, rightBlock });
}

// Whether evaluating an expression whether or not it is chosen is harmless: it
// reads no memory through an index, a pointer, a reference or a call, divides
// nothing and branches nowhere, so it cannot trap or touch an address the
// condition guards. A name bound to a buffer is such a pointer or reference,
// and so is every member reached through one.
bool Emitter::isSafeToEvaluateUnchosen(const Expression& expression) const {
	const auto safe = [this](const ExpressionPtr& child) {
		return !child || isSafeToEvaluateUnchosen(*child);
	};

	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
		case ExpressionKind::FloatLiteral:
		case ExpressionKind::BoolLiteral:
		case ExpressionKind::Sizeof:
			return true;
		case ExpressionKind::Identifier: {
			const Binding* binding = findResourceBinding(expression.name, expression.fileScope);
			return !binding || (binding->bufferPointeeType == InvalidId
				&& binding->storageClass != spirv::StorageClass::PhysicalStorageBuffer);
		}
		case ExpressionKind::Member:
			return safe(expression.left);
		case ExpressionKind::Unary:
			// ++ and -- write, and are not values to compute and discard.
			switch (expression.unaryOperator) {
				case UnaryOperator::Negate:
				case UnaryOperator::Plus:
				case UnaryOperator::Not:
				case UnaryOperator::BitNot:
					return safe(expression.left);
				default:
					return false;
			}
		case ExpressionKind::Binary:
			return expression.binaryOperator != BinaryOperator::Divide
				&& expression.binaryOperator != BinaryOperator::Modulo
				&& expression.binaryOperator != BinaryOperator::LogicalAnd
				&& expression.binaryOperator != BinaryOperator::LogicalOr
				&& safe(expression.left) && safe(expression.right);
		case ExpressionKind::Comma:
			return safe(expression.left) && safe(expression.right);
		case ExpressionKind::Construct:
		case ExpressionKind::Conditional: {
			if (!safe(expression.left)) {
				return false;
			}
			for (const ExpressionPtr& argument: expression.arguments) {
				if (!safe(argument)) {
					return false;
				}
			}
			return true;
		}
		case ExpressionKind::Index:
		case ExpressionKind::Call:
		case ExpressionKind::InitList:
		case ExpressionKind::Assign:
			return false;
	}

	return false;
}

// The type both values of a conditional have: the same type stays as it is,
// as in C++ where two shorts give a short, and two different scalars meet by
// the usual arithmetic conversions. A scalar beside a vector joins the
// vector's component type, and two vectors have to be one type.
Id Emitter::conditionalType(Id trueType, Id falseType) {
	if (trueType == falseType) {
		return trueType;
	}

	TypeTable::StructForm form;
	if (_types.structForm(trueType, form) || _types.structForm(falseType, form)) {
		// Forms of one struct convert to each other; convert() refuses anything else.
		return trueType;
	}

	if (_types.matrixInfo(trueType) || _types.matrixInfo(falseType)) {
		throw CompileError("the two values of a conditional are matrices of different types");
	}

	const uint32_t trueWidth = _types.vectorWidth(trueType);
	const uint32_t falseWidth = _types.vectorWidth(falseType);

	if (trueWidth > 1 && falseWidth > 1) {
		throw CompileError(trueWidth != falseWidth
			? "the two values of a conditional are vectors of different widths"
			: "the two values of a conditional are vectors of different types");
	}

	if (trueWidth > 1 || falseWidth > 1) {
		const Id vector = trueWidth > 1 ? trueType : falseType;
		const Id scalar = trueWidth > 1 ? falseType : trueType;
		if (_types.isFloat(scalar) && !_types.isFloat(vector)) {
			throw CompileError("a floating-point scalar cannot be combined with an integer vector");
		}
		return vector;
	}

	return usualArithmeticConversion(trueType, falseType).type;
}

Id Emitter::toConditionalType(Id value, Id type) {
	const Id from = _builder.typeOf(value);
	if (_types.vectorWidth(type) > 1 && _types.vectorWidth(from) == 1) {
		return broadcast(value, type);
	}

	return convert(value, from, type);
}

// `c ? a : b` evaluates one of its values. When neither can trap or touch
// memory, evaluating both and selecting is the same and costs no branch.
// Otherwise the values go in their own blocks and meet in an OpPhi, as in
// emitShortCircuit, so `i < n ? buf[i] : 0.0f` never reads buf[n].
Id Emitter::emitConditional(const Expression& expression) {
	checkConditionalOperands(*expression.left, *expression.arguments[0], *expression.arguments[1]);
	const Id condition = emitCondition(*expression.left);
	const Expression& whenTrue = *expression.arguments[0];
	const Expression& whenFalse = *expression.arguments[1];

	if (isSafeToEvaluateUnchosen(whenTrue) && isSafeToEvaluateUnchosen(whenFalse)) {
		Id trueValue = emitExpression(whenTrue);
		Id falseValue = emitExpression(whenFalse);
		const Id type = conditionalType(_builder.typeOf(trueValue), _builder.typeOf(falseValue));
		trueValue = toConditionalType(trueValue, type);
		falseValue = toConditionalType(falseValue, type);
		return _builder.emitTyped(spirv::OpSelect, type, { condition, trueValue, falseValue });
	}

	const Id trueLabel = _builder.nextId();
	const Id falseLabel = _builder.nextId();
	const Id mergeLabel = _builder.nextId();

	_builder.emit(spirv::OpSelectionMerge, { mergeLabel, kSelectionControlNone });
	terminate(spirv::OpBranchConditional, { condition, trueLabel, falseLabel });

	// The common type is known only once the second value is emitted, and the
	// first block must convert to it before it ends: emit the first, lift it
	// out unfinished, emit the second, then put the first back ahead of it.
	const size_t mark = _builder.functionsMark();
	beginBlock(trueLabel);
	Id trueValue = emitExpression(whenTrue);
	const Id trueEnd = _currentBlock;
	std::vector<spirv::Instruction> trueBody = _builder.takeFunctionsFrom(mark);

	beginBlock(falseLabel);
	Id falseValue = emitExpression(whenFalse);
	const Id falseEnd = _currentBlock;
	const Id type = conditionalType(_builder.typeOf(trueValue), _builder.typeOf(falseValue));
	falseValue = toConditionalType(falseValue, type);
	terminate(spirv::OpBranch, { mergeLabel });
	std::vector<spirv::Instruction> falseBody = _builder.takeFunctionsFrom(mark);

	_builder.appendFunctions(std::move(trueBody));
	trueValue = toConditionalType(trueValue, type);
	terminate(spirv::OpBranch, { mergeLabel });
	_builder.appendFunctions(std::move(falseBody));

	beginBlock(mergeLabel);
	return _builder.emitTyped(spirv::OpPhi, type, { trueValue, trueEnd, falseValue, falseEnd });
}

// Whether an expression may split into several blocks: a && or || on scalars,
// or a conditional whose values branch. Counted whether or not it does.
bool containsBranchingOperator(const Expression& expression) {
	if (expression.kind == ExpressionKind::Conditional
		|| (expression.kind == ExpressionKind::Binary
			&& (expression.binaryOperator == BinaryOperator::LogicalAnd
				|| expression.binaryOperator == BinaryOperator::LogicalOr))) {
		return true;
	}

	const auto contains = [](const ExpressionPtr& child) {
		return child && containsBranchingOperator(*child);
	};
	if (contains(expression.left) || contains(expression.right)) {
		return true;
	}
	for (const ExpressionPtr& argument: expression.arguments) {
		if (contains(argument)) {
			return true;
		}
	}
	return false;
}

Id Emitter::emitBinary(const Expression& expression) {
	checkBinaryComponentOperands(expression.binaryOperator, *expression.left, *expression.right);
	const Id left = emitExpression(*expression.left);

	const bool isLogical = expression.binaryOperator == BinaryOperator::LogicalAnd
		|| expression.binaryOperator == BinaryOperator::LogicalOr;
	const Id leftType = _builder.typeOf(left);
	if (isLogical && !_types.matrixInfo(leftType) && _types.vectorWidth(leftType) <= 1) {
		return emitShortCircuit(expression.binaryOperator, left, *expression.right);
	}

	const Id right = emitExpression(*expression.right);
	return emitBinaryOperation(expression.binaryOperator, left, right);
}

Id Emitter::emitBinaryOperation(BinaryOperator op, Id left, Id right) {
	const Id leftType = _builder.typeOf(left);

	if (_types.matrixInfo(leftType) || _types.matrixInfo(_builder.typeOf(right))) {
		return emitMatrixProduct(op, left, right);
	}

	if ((op == BinaryOperator::Equal || op == BinaryOperator::NotEqual)
		&& leftType == _boolType && _builder.typeOf(right) == _boolType) {
		const uint16_t opcode = op == BinaryOperator::Equal
			? spirv::OpLogicalEqual : spirv::OpLogicalNotEqual;
		return _builder.emitTyped(opcode, _boolType, { left, right });
	}

	const bool isLogical = op == BinaryOperator::LogicalAnd
		|| op == BinaryOperator::LogicalOr;

	// A comparison or logical operator yields a bool regardless of operand
	// type; an arithmetic one yields its operand type.
	const bool isComparison = !isLogical && isComparisonOperator(op);

	if (isLogical) {
		const uint16_t opcode = op == BinaryOperator::LogicalAnd
			? spirv::OpLogicalAnd : spirv::OpLogicalOr;
		if (_types.isBool(leftType) && leftType == _builder.typeOf(right)) {
			return _builder.emitTyped(opcode, leftType, { left, right });
		}

		return _builder.emitTyped(opcode, _boolType, { asCondition(left), asCondition(right) });
	}

	if (isComparison && (_types.vectorWidth(leftType) > 1 || _types.vectorWidth(_builder.typeOf(right)) > 1)) {
		return emitVectorComparison(op, left, right);
	}

	if (isComparison) {
		const ArithmeticConversion common = usualArithmeticConversion(_builder.typeOf(left), _builder.typeOf(right));
		const Id commonType = common.type;
		const bool operandsSigned = common.isSigned;
		Id leftOperand = left;
		Id rightOperand = right;

		if (_builder.typeOf(left) != commonType) {
			leftOperand = convert(left, _builder.typeOf(left), commonType);
		}
		if (_builder.typeOf(right) != commonType) {
			rightOperand = convert(right, _builder.typeOf(right), commonType);
		}

		return _builder.emitTyped(comparisonOpcode(op,
			_types.isFloat(commonType), operandsSigned), _boolType, { leftOperand, rightOperand });
	}

	// & | ^ act componentwise on a bool vector and give one (MSL 3.1): a scalar beside it is converted to bool
	// and widened. On booleans they are the logical and, or and inequality.
	const bool bitwise = op == BinaryOperator::BitAnd || op == BinaryOperator::BitOr
		|| op == BinaryOperator::BitXor;
	const Id rightType = _builder.typeOf(right);
	const bool leftBoolVector = _types.isBool(leftType) && _types.vectorWidth(leftType) > 1;
	const bool rightBoolVector = _types.isBool(rightType) && _types.vectorWidth(rightType) > 1;
	if (leftBoolVector || rightBoolVector) {
		if (!bitwise) {
			throw CompileError(std::string("operator ") + binaryOperatorSpelling(op)
				+ " on a bool vector is not lowered yet; only & | and ^ are");
		}
		const Id vectorType = leftBoolVector ? leftType : rightType;
		const auto operand = [&](Id value) {
			const Id type = _builder.typeOf(value);
			if (type == vectorType) {
				return value;
			}
			if (_types.vectorWidth(type) > 1) {
				throw CompileError("the operands of an operator are vectors of different widths or types");
			}
			// Bitwise operators take no floating-point operand (MSL 3.1), and a vector of an integral
			// element type takes no floating-point scalar beside it.
			if (_types.isFloat(type)) {
				throw CompileError(std::string("operator ") + binaryOperatorSpelling(op)
					+ " needs integer operands, and a floating-point scalar is beside a bool vector");
			}
			return broadcast(convert(value, type, _boolType), vectorType);
		};
		const uint16_t opcode = op == BinaryOperator::BitAnd ? spirv::OpLogicalAnd
			: op == BinaryOperator::BitOr ? spirv::OpLogicalOr : spirv::OpLogicalNotEqual;
		return _builder.emitTyped(opcode, vectorType, { operand(left), operand(right) });
	}

	// A scalar bool operand undergoes the integer promotion to int ([conv.prom]), as Apple's compiler
	// does: true | false is the int 1, and true * 2.0f the float 2.
	const Id intType = _types.scalar(ScalarKind::Int);
	if (_types.isBool(leftType)) {
		left = convert(left, leftType, intType);
	}
	if (_types.isBool(rightType)) {
		right = convert(right, rightType, intType);
	}

	return emitArithmetic(op, left, right);
}

// The operators + - * / % & | ^ << >> on numeric scalars and vectors, with
// the operand rules of C and of Apple's compiler. Scalars undergo the integer
// promotions and then the usual arithmetic conversions, so "int + float" adds
// in float. A vector keeps its own type: a scalar beside it is converted to
// the vector's component type, and two vectors have to be the same type.
Id Emitter::emitArithmetic(BinaryOperator op, Id left, Id right) {
	const Id leftType = _builder.typeOf(left);
	const Id rightType = _builder.typeOf(right);

	for (const Id type: { leftType, rightType }) {
		if (_types.bitWidth(type) == 0) {
			throw CompileError("an arithmetic or bitwise operator is lowered only for "
				"numeric scalars, vectors and matrices");
		}
	}

	const bool isShift = op == BinaryOperator::ShiftLeft
		|| op == BinaryOperator::ShiftRight;
	// % on a float is rejected by arithmeticOpcode, which knows to name fmod.
	const bool needsIntegers = isShift || op == BinaryOperator::BitAnd
		|| op == BinaryOperator::BitOr || op == BinaryOperator::BitXor;
	if (needsIntegers && (_types.isFloat(leftType) || _types.isFloat(rightType))) {
		throw CompileError(std::string("operator ") + binaryOperatorSpelling(op)
			+ " needs integer operands");
	}

	const uint32_t leftWidth = _types.vectorWidth(leftType);
	const uint32_t rightWidth = _types.vectorWidth(rightType);
	const auto emit = [&](Id type, Id leftOperand, Id rightOperand) {
		return _builder.emitTyped(arithmeticOpcode(op, _types.isFloat(type),
			_types.isSignedInt(type)), type, { leftOperand, rightOperand });
	};

	if (leftWidth > 1 && rightWidth > 1) {
		if (leftWidth != rightWidth) {
			throw CompileError("the operands of an operator are vectors of different widths");
		}

		// A shift count may be any integer type; every other operator wants the
		// same vector type on both sides.
		if (!isShift && leftType != rightType) {
			throw CompileError("an operator takes two vectors of the same type");
		}
		return emit(leftType, left, right);
	}

	if (leftWidth > 1) {
		if (_types.isFloat(rightType) && !_types.isFloat(leftType)) {
			throw CompileError("a floating-point scalar cannot be combined with an integer vector");
		}
		return emit(leftType, left, broadcast(right, leftType));
	}

	if (rightWidth > 1) {
		if (isShift) {
			throw CompileError("a scalar cannot be shifted by a vector");
		}
		if (_types.isFloat(leftType) && !_types.isFloat(rightType)) {
			throw CompileError("a floating-point scalar cannot be combined with an integer vector");
		}
		return emit(rightType, broadcast(left, rightType), right);
	}

	// A shift takes its type from the promoted left operand alone, and the
	// count keeps its own.
	if (isShift) {
		const Id promoted = _types.bitWidth(leftType) < 32 ? promotedTo(1) : leftType;
		return emit(promoted, convert(left, leftType, promoted), right);
	}

	const Id common = usualArithmeticConversion(leftType, rightType).type;
	return emit(common, convert(left, leftType, common), convert(right, rightType, common));
}

// A comparison with a vector operand is componentwise and yields a bool vector
// of the same width. The operand rules are those of the arithmetic operators: a
// scalar beside a vector is converted to its component type and broadcast, and
// two vectors have to be the same type.
Id Emitter::emitVectorComparison(BinaryOperator op, Id left, Id right) {
	const Id leftType = _builder.typeOf(left);
	const Id rightType = _builder.typeOf(right);

	// Two bool operands compare by value, as OpLogicalEqual does for scalars.
	const bool logical = (op == BinaryOperator::Equal || op == BinaryOperator::NotEqual)
		&& _types.isBool(leftType) && _types.isBool(rightType);

	if (_types.isBool(leftType) != _types.isBool(rightType)
		&& _types.vectorWidth(leftType) > 1 && _types.vectorWidth(rightType) > 1) {
		throw CompileError("implicit conversions between vector types are not permitted");
	}

	for (const Id type: { leftType, rightType }) {
		if (!logical && (_types.bitWidth(type) == 0 || _types.isBool(type))) {
			throw CompileError("a comparison with a vector operand is lowered only for "
				"numeric scalars and vectors; a bool operand is not lowered yet");
		}
	}

	const uint32_t leftWidth = _types.vectorWidth(leftType);
	const uint32_t rightWidth = _types.vectorWidth(rightType);
	Id operandType = leftType;

	if (leftWidth > 1 && rightWidth > 1) {
		if (leftWidth != rightWidth) {
			throw CompileError("the operands of an operator are vectors of different widths");
		}
		if (leftType != rightType) {
			throw CompileError("an operator takes two vectors of the same type");
		}
	} else {
		operandType = leftWidth > 1 ? leftType : rightType;
		const Id scalarType = leftWidth > 1 ? rightType : leftType;
		if (_types.isFloat(scalarType) && !_types.isFloat(operandType)) {
			throw CompileError("a floating-point scalar cannot be combined with an integer vector");
		}
		if (leftWidth > 1) {
			right = broadcast(right, operandType);
		} else {
			left = broadcast(left, operandType);
		}
	}

	const Id resultType = _types.vector(ScalarKind::Bool, _types.vectorWidth(operandType));
	if (logical) {
		return _builder.emitTyped(op == BinaryOperator::Equal
			? spirv::OpLogicalEqual : spirv::OpLogicalNotEqual, resultType, { left, right });
	}

	return _builder.emitTyped(comparisonOpcode(op, _types.isFloat(operandType),
		_types.isSignedInt(operandType)), resultType, { left, right });
}

// A product with a matrix on at least one side. SPIR-V has an opcode per
// shape, and each needs its operands to agree in component type and in the
// dimension the product runs over, so anything else is reported.
Id Emitter::emitMatrixProduct(BinaryOperator op, Id left, Id right) {
	if (op != BinaryOperator::Multiply) {
		throw CompileError("only * is lowered with a matrix operand; any other operator on a "
			"matrix is not lowered yet");
	}

	const Id leftType = _builder.typeOf(left);
	const Id rightType = _builder.typeOf(right);
	const auto* leftMatrix = _types.matrixInfo(leftType);
	const auto* rightMatrix = _types.matrixInfo(rightType);
	const std::string mismatch = "the operands of a matrix product do not match: the "
		"columns of the left have to equal the rows of the right, in the same component type";

	const std::string scaleMismatch = "a matrix is scaled by a scalar of its own component "
		"type, and this one is another type";

	// The scalar has to be the matrix's own component type already: Apple's
	// compiler rejects a float4x4 times an int or a half rather than converting.
	if (leftMatrix && _types.vectorWidth(rightType) == 1 && !rightMatrix) {
		if (rightType != _types.scalar(leftMatrix->scalar)) {
			throw CompileError(scaleMismatch);
		}
		return _builder.emitTyped(spirv::OpMatrixTimesScalar, leftType, { left, right });
	}

	if (rightMatrix && _types.vectorWidth(leftType) == 1 && !leftMatrix) {
		if (leftType != _types.scalar(rightMatrix->scalar)) {
			throw CompileError(scaleMismatch);
		}
		return _builder.emitTyped(spirv::OpMatrixTimesScalar, rightType, { right, left });
	}

	if (leftMatrix && rightMatrix) {
		if (leftMatrix->scalar != rightMatrix->scalar || leftMatrix->columns != rightMatrix->rows) {
			throw CompileError(mismatch);
		}

		return _builder.emitTyped(spirv::OpMatrixTimesMatrix,
			_types.matrix(leftMatrix->scalar, rightMatrix->columns, leftMatrix->rows),
			{ left, right });
	}

	if (leftMatrix) {
		if (_types.componentOf(rightType) != _types.scalar(leftMatrix->scalar)
			|| _types.vectorWidth(rightType) != leftMatrix->columns) {
			throw CompileError(mismatch);
		}

		return _builder.emitTyped(spirv::OpMatrixTimesVector, leftMatrix->column, { left, right });
	}

	if (_types.componentOf(leftType) != _types.scalar(rightMatrix->scalar)
		|| _types.vectorWidth(leftType) != rightMatrix->rows) {
		throw CompileError(mismatch);
	}

	return _builder.emitTyped(spirv::OpVectorTimesMatrix,
		_types.vector(rightMatrix->scalar, rightMatrix->columns), { left, right });
}

// "x op= v": x holds the current value and the result is what to store back.
// The operation is the binary operator's own, applied after the usual
// arithmetic conversions, so "int x; x += 1.5f" adds in float; the store
// converts the sum back to int.
Id Emitter::emitCompoundOperation(BinaryOperator op, Id current, Id value) {
	const Id targetType = _builder.typeOf(current);
	// A scalar bool value promotes to int; a bool target is still refused below.
	if (_builder.typeOf(value) == _boolType && targetType != _boolType) {
		value = convert(value, _boolType, promotedTo(1));
	}
	const Id valueType = _builder.typeOf(value);
	const std::string spelled = std::string(binaryOperatorSpelling(op)) + "=";

	// A bool operand, scalar or vector, follows the binary operator's own rules (a scalar is promoted to
	// int, a bool vector takes & | ^), and the store converts the result back to the target's type.
	const auto isBoolish = [&](Id type) { return type == _boolType || _types.componentOf(type) == _boolType; };
	if (isBoolish(targetType) || isBoolish(valueType)) {
		const Id result = emitBinaryOperation(op, current, value);
		return isBoolish(targetType) ? convert(result, _builder.typeOf(result), targetType) : result;
	}

	for (const Id type: { targetType, valueType }) {
		const bool isMatrix = _types.matrixInfo(type) != nullptr;
		const bool isNumeric = _types.bitWidth(type) != 0 && type != _boolType
			&& _types.componentOf(type) != _boolType;
		if (!isMatrix && !isNumeric) {
			throw CompileError("operator " + spelled + " is lowered only for numeric scalars, "
				"vectors and matrices; a bool, a pointer or a struct operand is not lowered yet");
		}
	}

	const auto* targetMatrix = _types.matrixInfo(targetType);
	const uint32_t targetWidth = _types.vectorWidth(targetType);
	const uint32_t valueWidth = _types.vectorWidth(valueType);

	// Apple's compiler scales a matrix in place by a scalar, converted to the
	// matrix's component type, and takes a matrix only on the right of a
	// vector ("v *= m"). A matrix times a matrix is not accepted in place.
	if (targetMatrix) {
		if (_types.matrixInfo(valueType) || valueWidth > 1) {
			throw CompileError("operator " + spelled + " takes a scalar on the right of a matrix");
		}
		value = convert(value, valueType, _types.scalar(targetMatrix->scalar));
	} else if (_types.matrixInfo(valueType)) {
		if (targetWidth == 1) {
			throw CompileError("operator " + spelled + " cannot take a matrix on the right of a scalar");
		}
	}
	if (targetMatrix || _types.matrixInfo(valueType)) {
		return emitBinaryOperation(op, current, value);
	}

	const bool isShift = op == BinaryOperator::ShiftLeft || op == BinaryOperator::ShiftRight;
	const bool needsIntegers = isShift || op == BinaryOperator::Modulo
		|| op == BinaryOperator::BitAnd || op == BinaryOperator::BitOr
		|| op == BinaryOperator::BitXor;
	if (needsIntegers && (_types.isFloat(targetType) || _types.isFloat(valueType))) {
		throw CompileError("operator " + spelled + " needs integer operands");
	}

	if (targetWidth > 1) {
		// A shift count is a vector of the same width, in any integer type; every
		// other operator wants the vector's own type.
		const bool countVector = isShift && valueWidth == targetWidth;
		if (valueWidth > 1 && valueType != targetType && !countVector) {
			throw CompileError("operator " + spelled + " on a vector takes a scalar or a vector "
				"of the same type");
		}
		if (valueWidth == 1 && _types.isFloat(valueType) && !_types.isFloat(targetType)) {
			throw CompileError("operator " + spelled + " cannot take a floating-point scalar on "
				"the right of an integer vector");
		}
		return emitBinaryOperation(op, current, value);
	}

	if (valueWidth > 1) {
		throw CompileError("operator " + spelled + " cannot take a vector on the right of a scalar");
	}

	return emitBinaryOperation(op, current, value);
}

} // namespace mslc
