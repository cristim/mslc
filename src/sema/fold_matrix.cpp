#include "sema/emitter.h"

namespace mslc {

FoldedConstant Emitter::foldInitializer(const Type& type, const Expression& initializer, bool braced) {
	if (type.isAtomic) {
		throw CompileError("a file-scope atomic constant is not lowered");
	}
	if (initializer.kind == ExpressionKind::InitList && initializer.constructType
		&& (type.namedType.empty() || initializer.constructType->namedType != type.namedType
			|| initializer.constructType->arrayLength || initializer.constructType->isPointer
			|| initializer.constructType->isVector() || initializer.constructType->isMatrix())) {
		throw CompileError("a constant of \"" + typeName(type) + "\" cannot be initialised from \""
			+ typeName(*initializer.constructType) + "\"");
	}
	if (type.isMatrix() && !type.isPointer) {
		return foldMatrix(initializer, &type);
	}

	if (initializer.kind != ExpressionKind::InitList) {
		if (!type.namedType.empty() && !type.isPointer) {
			throw CompileError("the struct \"" + type.namedType + "\" is initialised with a "
				"braced list naming its fields, not with a single value");
		}

		// "int4(1)" and "float4(a, b, c, d)": a vector built by its own constructor, each
		// component converted as it is at run time.
		if (type.isVector() && !type.isPointer && initializer.kind == ExpressionKind::Construct
			&& initializer.constructType && !initializer.constructType->isMatrix()) {
			const Type& built = *initializer.constructType;
			if (built.scalar != type.scalar || built.vectorWidth != type.vectorWidth) {
				throw CompileError("the constructor of a " + std::string(scalarKindName(type.scalar))
					+ std::to_string(type.vectorWidth) + " constant has to be a "
					+ std::string(scalarKindName(type.scalar)) + std::to_string(type.vectorWidth)
					+ " already; mslc converts no vector");
			}
			FoldedConstant vector;
			vector.isComposite = true;
			vector.type = declaredTypeOf(type);
			for (const FoldedConstant& component: foldVectorConstructor(initializer)) {
				vector.parts.push_back(emitConstant(component));
			}
			return vector;
		}

		if (type.isVector() && !type.isPointer) {
			throw CompileError("a " + std::string(scalarKindName(type.scalar))
				+ std::to_string(type.vectorWidth) + " constant is initialised with a braced "
				"list of its components or a constructor of its own type "
				"(not a single value, another constant or an expression)");
		}

		if (!type.isPointer) {
			requireEnumValue(type.enumName, initializer, "an element of the initializer list");
		}

		FoldedConstant folded = foldExpression(initializer);
		if (type.isPointer) {
			throw CompileError("a file-scope pointer constant is not lowered yet");
		}
		if (folded.isComposite) {
			throw CompileError("a composite cannot initialise a scalar");
		}

		// A bool is initialised with true or false and nothing else. OpConstant
		// has no literal form for it, so folding a number into one would emit
		// a word where the grammar allows none.
		if ((type.scalar == ScalarKind::Bool) != (folded.scalar == ScalarKind::Bool)) {
			throw CompileError(type.scalar == ScalarKind::Bool
				? "a bool constant is initialised with true or false"
				: std::string("a bool cannot initialise a ")
					+ std::string(scalarKindName(type.scalar)));
		}

		if (braced) {
			requireNotNarrowing(folded, type.scalar);
		}

		// The declared type is what the constant is. An integer literal may
		// initialise a float, since that is a widening; the other direction would
		// have to round, which is not something to do silently.
		if (isFloatKind(folded.scalar) && !isFloatKind(type.scalar)) {
			throw CompileError("a float cannot initialise a "
				+ std::string(scalarKindName(type.scalar)));
		}

		return convertConstant(folded, type.scalar);
	}

	if (type.isPointer) {
		throw CompileError("a file-scope pointer constant is not lowered yet");
	}

	if (type.isScalar() && type.namedType.empty() && !type.arrayLength) {
		if (initializer.elements.size() > 1) {
			throw CompileError("a " + std::string(scalarKindName(type.scalar))
				+ " constant takes one value in a brace list, found "
				+ std::to_string(initializer.elements.size()));
		}

		if (initializer.elements.empty()) {
			FoldedConstant zero;
			zero.scalar = type.scalar;
			return zero;
		}

		const InitializerElement& element = initializer.elements.front();
		if (!element.fieldName.empty()) {
			throw CompileError("a scalar's brace list holds its value, but \""
				+ element.fieldName + "\" names a field");
		}
		return foldInitializer(type, *element.value, true);
	}

	FoldedConstant folded;
	folded.isComposite = true;
	folded.type = declaredTypeOf(type);

	if (type.isVector()) {
		// A vector's components are positional, so the list is the vector in
		// order and each element is the vector's own component type.
		if (initializer.elements.size() != type.vectorWidth) {
			throw CompileError("a " + std::string(scalarKindName(type.scalar))
				+ std::to_string(type.vectorWidth) + " constant takes "
				+ std::to_string(type.vectorWidth) + " values, found "
				+ std::to_string(initializer.elements.size()));
		}

		for (const InitializerElement& element: initializer.elements) {
			if (!element.fieldName.empty()) {
				throw CompileError("a vector's components are given in order, but \""
					+ element.fieldName + "\" names one");
			}

			Type component = type;
			component.vectorWidth = 0;
			folded.parts.push_back(emitConstant(foldInitializer(component, *element.value, true)));
		}

		return folded;
	}

	const StructDecl* decl = _unit.findStruct(type.namedType);
	if (!decl) {
		throw CompileError("a constant of \"" + type.namedType
			+ "\" is not a struct this source declares");
	}

	foldStructInitializer(folded, *decl, initializer);
	return folded;
}

// A scalar element of a matrix constant, as the matrix's own element kind. In a braced
// list the value may not narrow; in a constructor it converts, as at run time.
double Emitter::foldMatrixScalar(const Expression& expression, ScalarKind kind, bool braced) {
	if (isMatrixExpression(expression) || (expression.kind == ExpressionKind::Construct
		&& expression.constructType && expression.constructType->vectorWidth > 1)
		|| expression.kind == ExpressionKind::InitList) {
		throw CompileError("a matrix element has to be a scalar here");
	}

	requireEnumValue("", expression, "a matrix constant element");
	const FoldedConstant folded = foldExpression(expression);
	if (folded.isComposite || folded.scalar == ScalarKind::Bool) {
		throw CompileError("a matrix or vector constant element has to be a float or half value; a bool or "
			"a composite is not folded into one");
	}
	if (braced) {
		requireNotNarrowing(folded, kind);
	}
	return convertConstant(folded, kind).number;
}

// float3(1, 2, 3), int4(1), int4(v2, 1, 2): the components of a vector constructor, each
// converted to the vector's own scalar as at run time. A piece may be a nested vector
// constructor; a count that is neither one scalar nor the width is refused.
std::vector<FoldedConstant> Emitter::foldVectorConstructor(const Expression& expression) {
	const Type& type = *expression.constructType;
	if (type.vectorWidth < 2 || type.isMatrix() || type.isPointer || type.isAtomic || type.arrayLength
		|| !type.namedType.empty() || !type.enumName.empty()) {
		throw CompileError("a constant's initialiser has to be a value that can be worked out "
			"from the declarations before it, and this is not one");
	}

	std::vector<FoldedConstant> pieces;
	for (const ExpressionPtr& argument: expression.arguments) {
		if (argument->kind == ExpressionKind::Construct && argument->constructType
			&& argument->constructType->vectorWidth > 1 && !argument->constructType->isMatrix()) {
			// Apple converts a vector piece of another scalar type only when it is the whole
			// argument list; beside other arguments it is an error, so it is refused here too.
			if (argument->constructType->scalar != type.scalar && expression.arguments.size() != 1) {
				throw CompileError("a vector piece of a constructor has to have the vector's own scalar "
					"type unless it is the only argument");
			}
			for (const FoldedConstant& nested: foldVectorConstructor(*argument)) {
				pieces.push_back(convertScalarForConstruct(nested, type.scalar));
			}
		} else {
			requireEnumValue("", *argument, "an argument of " + typeName(type));
			pieces.push_back(convertScalarForConstruct(foldExpression(*argument), type.scalar));
		}
	}

	if (pieces.size() == 1 && expression.arguments.size() == 1) {
		return std::vector<FoldedConstant>(type.vectorWidth, pieces.front());
	}
	if (pieces.size() != type.vectorWidth) {
		throw CompileError("a vector built from " + std::to_string(pieces.size())
			+ " components needs " + std::to_string(type.vectorWidth));
	}
	return pieces;
}

// A column written as a vector constructor: float3(1, 2, 3), float3(0), float3(v2, 1).
// The type has to be the column's own, as it does at run time.
std::vector<double> Emitter::foldColumnConstructor(const Expression& expression, ScalarKind kind,
	uint32_t rows) {

	const Type& type = *expression.constructType;
	if (type.scalar != kind || type.vectorWidth != rows) {
		throw CompileError("a column of this matrix has to be a " + std::string(scalarKindName(kind))
			+ std::to_string(rows) + " already; mslc converts no column");
	}

	std::vector<double> pieces;
	for (const ExpressionPtr& argument: expression.arguments) {
		if (argument->kind == ExpressionKind::Construct && argument->constructType
			&& argument->constructType->vectorWidth > 1 && !argument->constructType->isMatrix()) {
			const Type& inner = *argument->constructType;
			if (inner.scalar != kind) {
				throw CompileError("a vector piece of a column has to have its component type; "
					"only a scalar piece is converted");
			}
			const std::vector<double> nested = foldColumnConstructor(*argument, kind, inner.vectorWidth);
			pieces.insert(pieces.end(), nested.begin(), nested.end());
		} else {
			pieces.push_back(foldMatrixScalar(*argument, kind, false));
		}
	}

	if (pieces.size() == 1 && expression.arguments.size() == 1) {
		return std::vector<double>(rows, pieces.front());
	}
	if (pieces.size() != rows) {
		throw CompileError("a column built from " + std::to_string(pieces.size())
			+ " components needs " + std::to_string(rows));
	}
	return pieces;
}

bool Emitter::isMatrixExpression(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::Construct: return expression.constructType && expression.constructType->isMatrix();
		case ExpressionKind::Identifier: {
			if (!expression.fileScope && _bindings.count(expression.name)) { return false; }
			const auto found = _folded.find(expression.name);
			return found != _folded.end() && found->second.matrixColumns > 0;
		}
		case ExpressionKind::Binary:
			return isMatrixExpression(*expression.left) || isMatrixExpression(*expression.right);
		default: return false;
	}
}

// Emits the columns and the matrix as constants and keeps the numbers.
FoldedConstant Emitter::matrixConstant(ScalarKind kind, uint32_t columns, uint32_t rows,
	std::vector<double> elements) {

	FoldedConstant matrix;
	matrix.isComposite = true;
	matrix.scalar = kind;
	matrix.matrixColumns = columns;
	matrix.matrixRows = rows;
	matrix.type = _types.matrix(kind, columns, rows);
	for (uint32_t column = 0; column < columns; ++column) {
		FoldedConstant vector;
		vector.isComposite = true;
		vector.type = _types.vector(kind, rows);
		for (uint32_t row = 0; row < rows; ++row) {
			FoldedConstant element;
			element.scalar = kind;
			element.number = elements[column * rows + row];
			vector.parts.push_back(emitConstant(element));
		}
		matrix.parts.push_back(emitConstant(vector));
	}
	matrix.elements = std::move(elements);
	return matrix;
}

// float3x3(...) as a constant: a scalar for the diagonal, a matrix of its own shape,
// one vector constructor per column, or columns * rows scalars in column order.
FoldedConstant Emitter::foldMatrixConstruct(const Expression& expression) {
	const Type& type = *expression.constructType;
	const std::string spelled = typeName(type);
	const uint32_t columns = type.matrixColumns;
	const uint32_t rows = type.vectorWidth;
	std::vector<double> elements(static_cast<size_t>(columns) * rows, 0.0);
	const auto& arguments = expression.arguments;

	if (arguments.size() == 1) {
		if (isMatrixExpression(*arguments.front())) {
			const FoldedConstant from = foldMatrix(*arguments.front(), nullptr);
			if (from.matrixColumns != columns || from.matrixRows != rows) {
				throw CompileError("no matching conversion from a matrix of another shape to " + spelled);
			}
			for (size_t i = 0; i < elements.size(); ++i) {
				elements[i] = roundedToKind(type.scalar, from.elements[i]);
			}
		} else {
			const double diagonal = foldMatrixScalar(*arguments.front(), type.scalar, false);
			for (uint32_t i = 0; i < columns && i < rows; ++i) {
				elements[i * rows + i] = diagonal;
			}
		}
	} else if (arguments.size() == static_cast<size_t>(columns) * rows) {
		for (size_t i = 0; i < arguments.size(); ++i) {
			elements[i] = foldMatrixScalar(*arguments[i], type.scalar, false);
		}
	} else if (arguments.size() == columns) {
		for (uint32_t column = 0; column < columns; ++column) {
			const Expression& argument = *arguments[column];
			if (argument.kind != ExpressionKind::Construct || !argument.constructType
				|| argument.constructType->vectorWidth < 2 || argument.constructType->isMatrix()) {
				throw CompileError("a column of " + spelled + " has to be a "
					+ std::string(scalarKindName(type.scalar)) + std::to_string(rows)
					+ " constructor in a constant");
			}
			const std::vector<double> values = foldColumnConstructor(argument, type.scalar, rows);
			std::copy(values.begin(), values.end(), elements.begin() + column * rows);
		}
	} else if (!arguments.empty()) {
		throw CompileError("no matching constructor for " + spelled + " built from "
			+ std::to_string(arguments.size()) + " values");
	}

	return matrixConstant(type.scalar, columns, rows, std::move(elements));
}

// A braced list for a matrix initialises one column per element, as it does in Apple's
// compiler (measured on a GPU): a scalar fills the whole column, a vector is the column,
// a nested brace gives the column's components and has to give all of them, columns
// not written are zero, and elements past the last column are ignored.
FoldedConstant Emitter::foldMatrixList(const Type& type, const Expression& initializer) {
	const uint32_t columns = type.matrixColumns;
	const uint32_t rows = type.vectorWidth;
	std::vector<double> elements(static_cast<size_t>(columns) * rows, 0.0);
	for (size_t column = 0; column < columns && column < initializer.elements.size(); ++column) {
		const InitializerElement& element = initializer.elements[column];
		if (!element.fieldName.empty()) {
			throw CompileError("a matrix's columns are given in order, but \"" + element.fieldName
				+ "\" names one");
		}

		const Expression& value = *element.value;
		std::vector<double> values;
		if (value.kind == ExpressionKind::InitList) {
			if (value.constructType) {
				throw CompileError("a column of " + typeName(type) + " cannot be initialised from \""
					+ typeName(*value.constructType) + "\"");
			}
			if (value.elements.size() != rows) {
				throw CompileError("chosen constructor is explicit in copy-initialization: a column of "
					+ typeName(type) + " takes " + std::to_string(rows) + " values in braces, found "
					+ std::to_string(value.elements.size()));
			}
			for (const InitializerElement& component: value.elements) {
				if (!component.fieldName.empty() || component.value->kind == ExpressionKind::InitList) {
					throw CompileError("a column's components are scalars given in order");
				}
				values.push_back(foldMatrixScalar(*component.value, type.scalar, true));
			}
		} else if (value.kind == ExpressionKind::Construct && value.constructType
			&& value.constructType->vectorWidth > 1 && !value.constructType->isMatrix()) {
			values = foldColumnConstructor(value, type.scalar, rows);
		} else {
			values.assign(rows, foldMatrixScalar(value, type.scalar, true));
		}
		std::copy(values.begin(), values.end(), elements.begin() + column * rows);
	}

	return matrixConstant(type.scalar, columns, rows, std::move(elements));
}

// A * B of two constant matrices of one element kind: column j of the result is
// sum over k of A's column k times B[j][k], each product and each sum rounded to the
// element kind in turn, which is what Apple's compiler produced (measured on a GPU).
FoldedConstant Emitter::foldMatrixProduct(const Expression& expression) {
	if (expression.binaryOperator != BinaryOperator::Multiply) {
		throw CompileError("this operator on constant matrices is not folded yet; only a product is");
	}
	if (!isMatrixExpression(*expression.left) || !isMatrixExpression(*expression.right)) {
		throw CompileError("a matrix constant is multiplied by another matrix constant here; "
			"a product with a scalar or a vector is not folded yet");
	}

	const FoldedConstant a = foldMatrix(*expression.left, nullptr);
	const FoldedConstant b = foldMatrix(*expression.right, nullptr);
	if (a.scalar != b.scalar) {
		throw CompileError("the matrices multiplied have different element types");
	}
	if (a.matrixColumns != b.matrixRows) {
		throw CompileError("cannot multiply a " + std::to_string(a.matrixColumns) + "-column matrix by a "
			+ std::to_string(b.matrixRows) + "-row one");
	}

	const ScalarKind kind = a.scalar;
	const uint32_t columns = b.matrixColumns;
	const uint32_t rows = a.matrixRows;
	std::vector<double> elements;
	for (uint32_t j = 0; j < columns; ++j) {
		for (uint32_t i = 0; i < rows; ++i) {
			double sum = 0.0;
			for (uint32_t k = 0; k < a.matrixColumns; ++k) {
				const double product = roundedToKind(kind, a.elements[k * rows + i] * b.elements[j * b.matrixRows + k]);
				sum = k == 0 ? product : roundedToKind(kind, sum + product);
			}
			elements.push_back(sum);
		}
	}

	return matrixConstant(kind, columns, rows, std::move(elements));
}

// A matrix constant's initialiser: a braced list for the declared type, a constructor,
// the name of a matrix constant declared above, or a product of two of those.
FoldedConstant Emitter::foldMatrix(const Expression& expression, const Type* expected) {
	FoldedConstant folded;
	switch (expression.kind) {
		case ExpressionKind::InitList:
			if (expression.constructType) {
				throw CompileError("a matrix constant is initialised with a matrix, not a "
					+ typeName(*expression.constructType));
			}
			if (!expected) {
				throw CompileError("a braced list needs the matrix type it initialises");
			}
			folded = foldMatrixList(*expected, expression);
			break;
		case ExpressionKind::Construct:
			if (!expression.constructType || !expression.constructType->isMatrix()) {
				throw CompileError("a matrix constant is initialised with a matrix, not a "
					+ (expression.constructType ? typeName(*expression.constructType) : std::string("value")));
			}
			folded = foldMatrixConstruct(expression);
			break;
		case ExpressionKind::Identifier: {
			const auto found = (!expression.fileScope && _bindings.count(expression.name))
				? _folded.end() : _folded.find(expression.name);
			if (found == _folded.end() || found->second.matrixColumns == 0) {
				throw CompileError("\"" + expression.name + "\" is not a matrix constant this source "
					"declares before this one");
			}
			folded = found->second;
			break;
		}
		case ExpressionKind::Binary:
			folded = foldMatrixProduct(expression);
			break;
		default:
			throw CompileError("a matrix constant's initialiser has to be a matrix constructor, a braced list "
				"or a product of matrix constants");
	}

	if (expected && (folded.scalar != expected->scalar || folded.matrixColumns != expected->matrixColumns
		|| folded.matrixRows != expected->vectorWidth)) {
		throw CompileError("no viable conversion to " + typeName(*expected) + " from a matrix of another type");
	}
	return folded;
}

} // namespace mslc
