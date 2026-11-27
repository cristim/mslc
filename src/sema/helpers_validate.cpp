#include "sema/emitter.h"

namespace mslc {

FoldedConstant convertConstant(FoldedConstant constant, ScalarKind to) {
	if (to == ScalarKind::Bool || constant.scalar == ScalarKind::Bool) {
		return constant;
	}

	if (isFloatKind(to)) {
		constant.number = isFloatKind(constant.scalar) ? roundedToKind(to, constant.number)
			: integerAsKind(to, constant.scalar, constant.integer);
	} else {
		constant.integer = normalizeInteger(to, constant.integer);
	}

	constant.scalar = to;
	return constant;
}

const char* cxxTypeName(ScalarKind kind) {
	switch (kind) {
		case ScalarKind::Char: return "char";
		case ScalarKind::UChar: return "unsigned char";
		case ScalarKind::Short: return "short";
		case ScalarKind::UShort: return "unsigned short";
		case ScalarKind::Int: return "int";
		case ScalarKind::UInt: return "unsigned int";
		case ScalarKind::Long: return "long";
		case ScalarKind::ULong: return "unsigned long";
		default: return scalarKindName(kind);
	}
}

std::string integerText(const FoldedConstant& constant) {
	return isSignedInteger(constant.scalar) ? std::to_string(static_cast<int64_t>(constant.integer))
		: std::to_string(constant.integer);
}

// A value inside a braced initialiser has to reach its element type without
// changing, as in C++11, and Apple's compiler makes it an error where a
// plain "constant int k = 2147483648;" is a warning. An integer has to fit
// an integer type and be exactly representable in a float type, a float is
// never narrowed to an integer, and a float only has to be in range for a
// narrower float.
void requireNotNarrowing(const FoldedConstant& constant, ScalarKind to) {
	const ScalarKind from = constant.scalar;
	const auto narrowed = [&]() {
		return CompileError("constant expression evaluates to " + (isFloatKind(from)
			? std::to_string(constant.number) : integerText(constant))
			+ " which cannot be narrowed to type '" + cxxTypeName(to) + "' in an initializer list");
	};

	if (isFloatKind(from)) {
		if (!isFloatKind(to)) {
			throw CompileError("type '" + std::string(cxxTypeName(from)) + "' cannot be narrowed to '"
				+ cxxTypeName(to) + "' in an initializer list");
		}

		if (std::isfinite(constant.number) && !std::isfinite(roundedToKind(to, constant.number))) {
			throw narrowed();
		}

		return;
	}

	const bool fromSigned = isSignedInteger(from);
	const bool negative = fromSigned && static_cast<int64_t>(constant.integer) < 0;
	const bool beyondLong = !fromSigned && static_cast<int64_t>(constant.integer) < 0;

	if (isFloatKind(to)) {
		// Apple, as clang does, converts the float back to the integer's own type
		// and compares. The conversion back saturates, so INT_MAX, which rounds
		// to 2^31 as a float and to infinity as a half, comes back as INT_MAX
		// and is let through.
		const double converted = integerAsKind(to, from, constant.integer);
		const __int128 value = fromSigned ? static_cast<__int128>(static_cast<int64_t>(constant.integer))
			: static_cast<__int128>(constant.integer);
		const __int128 width = static_cast<__int128>(1) << (mappingFor(from).width - (fromSigned ? 1 : 0));
		const __int128 low = fromSigned ? -width : 0;
		const __int128 high = width - 1;
		__int128 back = 0;
		if (converted >= static_cast<double>(high)) {
			back = high;
		} else if (converted <= static_cast<double>(low)) {
			back = low;
		} else {
			back = static_cast<__int128>(converted);
		}
		const bool exact = back == value;
		if (!exact) {
			throw narrowed();
		}

		return;
	}

	const bool toSigned = isSignedInteger(to);
	if ((negative && !toSigned) || (beyondLong && toSigned)
		|| normalizeInteger(to, constant.integer) != constant.integer) {
		throw narrowed();
	}
}

// Whether an integer case value is one the selector's type holds, which C++
// asks of a case label where a braced initialiser asks it of a value.
bool fitsInKind(const FoldedConstant& constant, ScalarKind to) {
	const uint32_t width = mappingFor(to).width;
	if (isSignedInteger(constant.scalar)) {
		const int64_t value = static_cast<int64_t>(constant.integer);
		if (isSignedInteger(to)) {
			if (width >= 64) {
				return true;
			}
			const int64_t low = -(int64_t{ 1 } << (width - 1));
			const int64_t high = (int64_t{ 1 } << (width - 1)) - 1;
			return value >= low && value <= high;
		}
		if (value < 0) {
			return false;
		}
		return width >= 64 || static_cast<uint64_t>(value) <= ((uint64_t{ 1 } << width) - 1);
	}

	const uint64_t value = constant.integer;
	if (isSignedInteger(to)) {
		return width >= 64 ? static_cast<int64_t>(value) >= 0
			: value <= ((uint64_t{ 1 } << (width - 1)) - 1);
	}
	return width >= 64 || value <= ((uint64_t{ 1 } << width) - 1);
}


void collectCalls(const Expression& expression, std::vector<const Expression*>& out) {
	if (expression.kind == ExpressionKind::Call) {
		out.push_back(&expression);
	}

	for (const ExpressionPtr* child: { &expression.left, &expression.right }) {
		if (*child) {
			collectCalls(**child, out);
		}
	}
	for (const ExpressionPtr& argument: expression.arguments) {
		collectCalls(*argument, out);
	}
	for (const InitializerElement& element: expression.elements) {
		collectCalls(*element.value, out);
	}
}

void collectCalls(const VariableDeclaration& declaration, std::vector<const Expression*>& out) {
	if (declaration.initializer) {
		collectCalls(*declaration.initializer, out);
	}
}

void collectCalls(const Statement& statement, std::vector<const Expression*>& out) {
	for (const StatementPtr& child: statement.children) {
		collectCalls(*child, out);
	}
	for (const ExpressionPtr* expression: { &statement.expression, &statement.forCondition,
		&statement.forIncrement, &statement.whileCondition }) {
		if (*expression) {
			collectCalls(**expression, out);
		}
	}
	if (statement.declaration) {
		collectCalls(*statement.declaration, out);
	}
	if (statement.forInitializer) {
		collectCalls(*statement.forInitializer, out);
	}
	for (const StatementPtr* branch: { &statement.thenBranch, &statement.elseBranch,
		&statement.forBody, &statement.whileBody }) {
		if (*branch) {
			collectCalls(**branch, out);
		}
	}
	for (const SwitchCase& kase: statement.switchCases) {
		if (kase.value) {
			collectCalls(*kase.value, out);
		}
		for (const StatementPtr& child: kase.body) {
			collectCalls(*child, out);
		}
	}
	for (const StatementPtr& child: statement.switchPreamble) {
		collectCalls(*child, out);
	}
}


// The names the unit gives two or more signatures. A member function keeps one
// signature per name, which validateHelpers still enforces.
std::set<std::string> overloadedNames(const TranslationUnit& unit) {
	std::map<std::string, const FunctionDecl*> seen;
	std::set<std::string> overloaded;
	for (const FunctionDecl& helper: unit.helpers) {
		const size_t tail = helper.name.rfind("::");
		if (tail != std::string::npos && unit.findStruct(helper.name.substr(0, tail))) {
			continue;
		}
		const auto previous = seen.emplace(helper.name, &helper);
		if (!previous.second && !sameParameterTypes(*previous.first->second, helper)) {
			overloaded.insert(helper.name);
		}
	}
	return overloaded;
}

// What a function is called inside the compiler: its name, and for an overloaded
// name also its parameters, so each signature is a function of its own.
std::string helperKey(const FunctionDecl& helper, const std::set<std::string>& overloaded) {
	return overloaded.count(helper.name) ? helper.name + "(" + parameterSignature(helper) + ")" : helper.name;
}


void validateHelpers(const TranslationUnit& unit, bool allowImports) {
	const std::set<std::string> overloaded = overloadedNames(unit);
	std::map<std::string, const FunctionDecl*> first;
	std::map<std::string, const FunctionDecl*> defined;

	for (const FunctionDecl& helper: unit.helpers) {
		const auto declared = [&](const Type& type) {
			if (!type.namedType.empty() && !unit.findStruct(type.namedType)) {
				throw CompileError("undeclared type \"" + type.namedType + "\" in helper function \""
					+ helper.name + "\"");
			}
		};
		declared(helper.returnType);
		for (const Parameter& parameter: helper.parameters) {
			declared(parameter.type);
		}

		if (findMathBuiltin(helper.name) || findPackBuiltin(helper.name)) {
			throw CompileError("helper function \"" + helper.name + "\" has the name of a builtin "
				"function; Apple's compiler reports a call that matches both as ambiguous, "
				"which mslc does not model");
		}
		if (unit.findFunction(helper.name)) {
			throw CompileError("\"" + helper.name + "\" names both an entry point and a helper function");
		}

		// A call to an overloaded name is resolved by its arguments' types, which only
		// the emitter has: the by-name checks below skip it (no key is the bare name).
		const std::string key = helperKey(helper, overloaded);
		const auto previous = first.emplace(key, &helper);
		if (!previous.second) {
			if (!mslc::sameParameterTypes(*previous.first->second, helper)) {
				throw CompileError("overloading \"" + helper.name + "\" is not lowered yet; Apple "
					"accepts a function declared again with other parameters, mslc takes one "
					"signature per name");
			}

			const Type& was = previous.first->second->returnType;
			const Type& now = helper.returnType;
			if (was.scalar != now.scalar || was.vectorWidth != now.vectorWidth
				|| was.matrixColumns != now.matrixColumns || was.namedType != now.namedType) {
				throw CompileError("\"" + helper.name + "\" is declared again with a different "
					"return type");
			}
		}

		if (helper.body && !defined.emplace(key, &helper).second) {
			throw CompileError("redefinition of \"" + helper.name + "\"");
		}

		// A prototype in namespace metal redeclares a standard library function; a
		// definition with one of its parameter lists redefines it.
		if (helper.body && helper.name.compare(0, stdlibPrefix.size(), stdlibPrefix) == 0) {
			std::vector<std::optional<StdlibOperand>> operands;
			for (const Parameter& parameter: helper.parameters) {
				operands.push_back(stdlibOperand(parameter));
			}
			if (isStdlibSignature(helper.name.substr(stdlibPrefix.size()), operands)) {
				throw CompileError("redefinition of \"" + helper.name + "\": the standard library defines it "
					"with these parameters");
			}
		}
	}

	std::map<std::string, std::set<std::string>> calls;
	const auto check = [&](const FunctionDecl& caller) {
		std::vector<const Expression*> found;
		collectCalls(*caller.body, found);

		for (const Expression* call: found) {
			if (call->left->kind != ExpressionKind::Identifier) {
				continue;
			}

			const std::string& name = call->left->name;
			if (unit.findFunction(name)) {
				throw CompileError("cannot call the entry point \"" + name + "\"");
			}
			// Several names are one overload set, chosen by the arguments at emission.
			if (!call->left->overloadNames.empty()) {
				continue;
			}
			// A template declared above the caller competes with the helper; one below is no candidate.
			if (std::any_of(unit.templates.begin(), unit.templates.end(), [&](const FunctionTemplate& declaration) {
				return declaration.name == name && declaration.order <= caller.order;
			})) {
				continue;
			}

			const auto declared = first.find(name);
			if (declared == first.end()) {
				continue;
			}

			if (declared->second->order > caller.order) {
				throw CompileError("\"" + name + "\" is called before it is declared; declare it "
					"above its first use");
			}
			if (call->arguments.size() != declared->second->parameters.size()) {
				throw CompileError("call to \"" + name + "\" passes " + std::to_string(call->arguments.size())
					+ " arguments, and it takes " + std::to_string(declared->second->parameters.size()));
			}
			// A Linkage object may call a function it only declares: the definition
			// comes from another translation unit at link time. A Vulkan executable
			// cannot, since nothing would resolve it.
			if (!defined.count(name) && !allowImports) {
				throw CompileError("\"" + name + "\" is declared but never defined");
			}

			if (caller.isHelper()) {
				calls[caller.name].insert(name);
			}
		}
	};

	for (const FunctionDecl& function: unit.functions) {
		check(function);
	}
	for (const auto& [key, definition]: defined) {
		check(*definition);
	}

	enum class Visit { Open, Done };
	std::map<std::string, Visit> state;
	std::vector<std::string> path;
	const auto visit = [&](const auto& self, const std::string& name) -> void {
		const auto known = state.find(name);
		if (known != state.end()) {
			if (known->second == Visit::Done) {
				return;
			}

			std::string cycle;
			const auto start = std::find(path.begin(), path.end(), name);
			for (auto it = start; it != path.end(); ++it) {
				cycle += *it + " -> ";
			}
			throw CompileError("recursive call: " + cycle + name + "; SPIR-V for Vulkan forbids "
				"recursion (Apple's compiler accepts it)");
		}

		state[name] = Visit::Open;
		path.push_back(name);
		for (const std::string& callee: calls[name]) {
			self(self, callee);
		}
		path.pop_back();
		state[name] = Visit::Done;
	};
	for (const auto& [key, definition]: defined) {
		visit(visit, definition->name);
	}
}

bool mentionsName(const Expression* expression, const std::string& name) {
	if (!expression) { return false; }
	if (expression->kind == ExpressionKind::Identifier && expression->name == name) { return true; }
	if (mentionsName(expression->left.get(), name) || mentionsName(expression->right.get(), name)) { return true; }
	for (const ExpressionPtr& argument: expression->arguments) {
		if (mentionsName(argument.get(), name)) { return true; }
	}
	for (const InitializerElement& element: expression->elements) {
		if (mentionsName(element.value.get(), name)) { return true; }
	}
	return false;
}

bool mentionsName(const Statement* statement, const std::string& name) {
	if (!statement) { return false; }
	if (mentionsName(statement->expression.get(), name) || mentionsName(statement->forCondition.get(), name)
		|| mentionsName(statement->forIncrement.get(), name) || mentionsName(statement->whileCondition.get(), name)
		|| mentionsName(statement->thenBranch.get(), name) || mentionsName(statement->elseBranch.get(), name)
		|| mentionsName(statement->forBody.get(), name) || mentionsName(statement->whileBody.get(), name)) { return true; }
	if (statement->declaration && mentionsName(statement->declaration->initializer.get(), name)) { return true; }
	if (statement->forInitializer && mentionsName(statement->forInitializer->initializer.get(), name)) { return true; }
	for (const StatementPtr& child: statement->children) {
		if (mentionsName(child.get(), name)) { return true; }
	}
	for (const SwitchCase& kase: statement->switchCases) {
		for (const StatementPtr& child: kase.body) {
			if (mentionsName(child.get(), name)) { return true; }
		}
	}
	return false;
}

} // namespace mslc
