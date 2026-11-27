#include "parser/internal.h"

namespace mslc {

// "template <...> struct Name { ... };" at file scope. The body is kept as tokens: nothing in it means
// anything until the parameters are bound, so a template that is never named costs nothing, as in clang.
void Parser::parseClassTemplate(ClassTemplate& declaration) {
	if (!atKeyword("struct")) {
		throw CompileError("a class or union template is not lowered; write it as a struct");
	}
	const size_t first = _position;
	advance();
	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected the name of a class template, found " + std::string(tokenKindName(kind())));
	}
	declaration.spelled = std::string(advance().text);
	const std::string quoted = "the class template \"" + declaration.spelled + "\"";
	if (at(TokenKind::Less)) {
		throw CompileError("a partial or explicit specialization of " + quoted + " is not lowered; Apple accepts "
			"it, and mslc refuses it so that the primary template is never silently used in its place");
	}
	if (at(TokenKind::Semicolon)) {
		throw CompileError("a declaration of " + quoted + " without its definition is not lowered");
	}
	if (!at(TokenKind::LBrace)) {
		throw CompileError("expected the body of " + quoted + "; a base list is not lowered");
	}
	skipBalanced(TokenKind::LBrace, TokenKind::RBrace);
	if (!at(TokenKind::Semicolon)) {
		throw CompileError("a declarator after the body of " + quoted + " is not lowered");
	}
	advance();
	declaration.tokens.assign(_tokens.begin() + first, _tokens.begin() + _position);
	declaration.tokens.emplace_back();

	declaration.name = qualify(declaration.spelled);
	declaration.namespacePath = _namespacePath;
	declaration.order = _functionOrder++;
	declaration.globalsBefore = _unit.globals.size();
	for (const auto& entry: _declared) {
		declaration.declaredBefore.insert(entry.first);
	}
	declaration.declaredBefore.insert(declaration.name);
	for (const Token& token: declaration.tokens) {
		if (token.kind == TokenKind::Identifier) {
			declaration.mentioned.insert(std::string(token.text));
		}
	}
	for (const TemplateParameter& parameter: declaration.parameters) {
		for (const Token& token: parameter.defaultTokens) {
			if (token.kind == TokenKind::Identifier) {
				declaration.mentioned.insert(std::string(token.text));
			}
		}
	}
	// A namespace alias the template names: "A::h(v)" with "namespace A = N;" reads N. Which alias a qualified or
	// directive-reached spelling means is not resolved, so every alias that exists at the definition under a name
	// the template mentions counts (one declared below the template cannot be named by it). A same-named alias in
	// an unrelated namespace is refused too (a documented over-refusal).
	for (const auto& alias: _namespaceAliases) {
		const size_t cut = alias.first.rfind("::");
		if (declaration.mentioned.count(cut == std::string::npos ? alias.first : alias.first.substr(cut + 2))) {
			declaration.aliasTargets.push_back(alias.second);
		}
	}
	declaration.usingsBefore = usingCount(_namespacePath, declaration.mentioned, declaration.aliasTargets);
	for (const std::string& name: declaration.mentioned) {
		declaration.usingDeclarationsBefore[name] = usingDeclarationsOf(name);
	}
	declareName(declaration.name, "class template");
	const std::string name = declaration.name;
	_classTemplates[name] = std::make_shared<const ClassTemplate>(std::move(declaration));
}

const ClassTemplate* Parser::classTemplateNamed(const std::vector<std::string>& parts, bool global) const {
	if (_classTemplates.empty()) {
		return nullptr;
	}
	std::string resolved;
	if (parts.size() == 1 && !global) {
		if (memberHidesName(parts[0])) {
			return nullptr;
		}
		if (const std::string* bound = blockName(parts[0]); bound && bound->empty()) {
			return nullptr;
		} else if (bound) {
			resolved = *bound;
		}
	}
	if (resolved.empty()) {
		QualifiedName name;
		name.parts = parts;
		name.global = global;
		try {
			resolved = resolveName(name);
		} catch (const CompileError&) {
			return nullptr;
		}
	}
	const auto found = _classTemplates.find(resolved);
	return found == _classTemplates.end() ? nullptr : found->second.get();
}

// "<" arguments ">" after the name of a class template, the "<" being the current token.
std::vector<TemplateArgument> Parser::parseClassTemplateArguments(const ClassTemplate& declaration) {
	expect(TokenKind::Less, "to open template arguments");
	std::vector<TemplateArgument> arguments;
	if (!at(TokenKind::Greater) && !at(TokenKind::ShiftRight)) {
		do {
			arguments.push_back(parseTemplateArgument());
		} while (match(TokenKind::Comma));
	}
	closeAngle(("to close the template arguments of \"" + declaration.name + "\"").c_str());
	return arguments;
}

// Makes each seed whose class template and argument structs exist and that is not made yet, in order, at the
// start of a declaration. The instance is read as if the program had spelled it here: the same order and
// the same refusals of a name or a using declaration the class template did not see.
void Parser::makeSeeds() {
	for (size_t i = 0; i < _seeds.size(); ++i) {
		if (_seeded[i]) {
			continue;
		}
		const ClassInstanceSeed& seed = _seeds[i];
		const auto found = _classTemplates.find(seed.className);
		if (found == _classTemplates.end()) {
			break;
		}
		bool known = true;
		for (const TemplateArgument& argument: seed.arguments) {
			known = known && (argument.isType ? argument.type.namedType.empty() || _unit.findStruct(argument.type.namedType)
				: true);
		}
		if (!known) {
			break;
		}
		_seeded[i] = true;
		try {
			instantiateClass(*found->second, seed.arguments);
		} catch (const CompileError& error) {
			throw ClassTemplateError(std::string("the instance \"") + seed.instance + "\", named by a function "
				"template: " + error.what());
		}
	}
}

// The struct "Name<arguments>", declared as the template's tokens read with the arguments bound. It is
// an ordinary struct once declared: its fields and its member functions and constructors, which become
// helpers named after it, are read where the template is used, and each distinct argument list is one
// struct. A member function sees what the template's declaration sees: it counts as declared where the
// template is, so a function written after the template is not one it can call.
std::string Parser::instantiateClass(const ClassTemplate& declaration, std::vector<TemplateArgument> arguments) {
	const std::string quoted = "the class template \"" + declaration.name + "\"";
	const std::vector<TemplateParameter>& parameters = declaration.parameters;
	if (arguments.size() > parameters.size()) {
		throw ClassTemplateError(quoted + " takes " + std::to_string(parameters.size()) + " arguments, and "
			+ std::to_string(arguments.size()) + " are given");
	}
	bool dependent = false;
	for (const TemplateArgument& argument: arguments) {
		if (argument.unbound) {
			throw DependentClassInstance("an instance of " + quoted + " that depends on a constant template parameter of a "
				"function template is not lowered yet");
		}
		dependent = dependent || (argument.isType && isDeductionMarker(argument.type));
	}
	if (dependent && arguments.size() < parameters.size()) {
		throw ClassTemplateError("an instance of " + quoted + " that depends on the parameters of a function "
			"template and leaves an argument to its default is not lowered yet");
	}

	// Saves what the binding of a parameter replaces, and puts it back.
	struct Bindings {
		Parser& parser;
		struct Saved {
			std::string key;
			std::optional<Type> type;
			std::optional<EnumConstant> constant;
		};
		std::vector<Saved> saved;
		Bindings(Parser& owner, const std::vector<TemplateParameter>& parameters): parser(owner) {
			for (const TemplateParameter& parameter: parameters) {
				Saved entry;
				entry.key = "<template " + parameter.name + ">";
				if (const auto type = parser._typedefs.find(entry.key); type != parser._typedefs.end()) {
					entry.type = type->second;
				}
				if (const auto constant = parser._enumConstants.find(entry.key); constant != parser._enumConstants.end()) {
					entry.constant = constant->second;
				}
				saved.push_back(std::move(entry));
			}
		}
		~Bindings() {
			for (const Saved& entry: saved) {
				entry.type ? void(parser._typedefs[entry.key] = *entry.type) : void(parser._typedefs.erase(entry.key));
				entry.constant ? void(parser._enumConstants[entry.key] = *entry.constant)
					: void(parser._enumConstants.erase(entry.key));
			}
		}
	};
	struct Guard {
		Parser& parser;
		Context saved;
		Guard(Parser& owner, Context&& next): parser(owner), saved(std::move(next)) { parser.swapContext(saved); }
		~Guard() { parser.swapContext(saved); }
	};

	struct Defined {
		Parser& parser;
		const std::set<std::string>* saved;
		Defined(Parser& owner, const std::set<std::string>* names): parser(owner), saved(owner._definedNames) {
			parser._definedNames = names;
		}
		~Defined() { parser._definedNames = saved; }
	};
	const auto fresh = [&](std::vector<Token> tokens) {
		Context next;
		next.tokens = std::move(tokens);
		reserveSplits(next.tokens);
		next.namespacePath = declaration.namespacePath;
		next.blocks.emplace_back();
		return next;
	};

	try {
		for (size_t index = arguments.size(); index < parameters.size(); ++index) {
			const TemplateParameter& parameter = parameters[index];
			if (!parameter.hasDefault) {
				throw CompileError("too few template arguments for " + quoted + ": \"" + parameter.name
					+ "\" has no default");
			}
			// Only the parameters before this one are in scope.
			const Bindings bindings(*this, parameters);
			const Defined defined(*this, &declaration.declaredBefore);
			const Guard guard(*this, fresh(parameter.defaultTokens));
			bindParameters(declaration.name, std::vector<TemplateParameter>(parameters.begin(), parameters.begin() + index),
				arguments);
			TemplateArgument argument;
			argument.isType = parameter.isType;
			if (parameter.isType) {
				argument.type = parseType();
				if (argument.type.isPointer || argument.type.arrayLength || argument.type.resource != ResourceKind::None
					|| !argument.type.enumName.empty() || argument.type.addressSpace != AddressSpace::None
					|| argument.type.isVolatile) {
					throw CompileError("a default that is a pointer, an array, a resource, an enum, volatile or in "
						"an address space is not lowered");
				}
			} else {
				const ExpressionPtr value = parseAssignment();
				argument.value = value->kind == ExpressionKind::BoolLiteral ? (value->boolValue ? 1 : 0)
					: evaluateConstant(*value, false);
			}
			if (!at(TokenKind::EndOfFile)) {
				throw CompileError("unexpected \"" + std::string(current().text) + "\" in a default argument");
			}
			if (argument.isType && isDeductionMarker(argument.type)) {
				throw CompileError("a default that depends on a function template parameter is not lowered yet");
			}
			arguments.push_back(std::move(argument));
		}
	} catch (const CompileError& error) {
		throw ClassTemplateError("in " + quoted + " (declared at line " + std::to_string(declaration.line) + "): "
			+ error.what());
	}

	// "::" separates a struct's namespace from its members everywhere, so an argument's own is not spelled.
	std::string key;
	for (size_t i = 0; i < arguments.size(); ++i) {
		const TemplateArgument& argument = arguments[i];
		std::string text = argument.isType ? typeName(argument.type)
			: std::to_string(argument.value);
		for (size_t at = text.find("::"); at != std::string::npos; at = text.find("::", at + 1)) {
			text.replace(at, 2, "$");
		}
		key += (i ? "," : "") + text;
	}
	const std::string spelled = declaration.spelled + "<" + key + ">";
	const std::string instance = declaration.name.substr(0, declaration.name.size() - declaration.spelled.size()) + spelled;
	if (dependent) {
		// While a function template's signature is read to deduce its parameters, an instance over one of them
		// is a marker type, like the parameter itself; deduction matches an argument's struct against what it
		// records. No struct is made.
		Type marker;
		marker.namedType = "<" + instance + ">";
		_typedefs[instance] = marker;
		_declared[instance] = "typedef";
		_dependentInstances[marker.namedType] = DependentInstance { declaration.name, arguments };
		return instance;
	}
	if (_unit.findStruct(instance) || _classInProgress.count(instance)) {
		return instance;
	}
	if (_frozenClasses) {
		if (_seedKeys.count(instance)) {
			throw ClassTemplateError("the instance \"" + instance + "\" of " + quoted + " is named inside a function "
				"template, but its argument types are declared after the template; declare them above it");
		}
		throw NeedClassInstance({ declaration.name, arguments, instance });
	}
	// clang reads a name from the definition, and a function again by argument-dependent lookup where the
	// instance is made. Neither is modelled for what was declared after the template, so a program that
	// could tell the two apart is refused rather than given another function's or constant's value.
	if (usingCount(declaration.namespacePath, declaration.mentioned, declaration.aliasTargets) != declaration.usingsBefore) {
		throw ClassTemplateError("a using declaration or directive is made after " + quoted + " in a scope it reads "
			"names from; write it above the template");
	}
	{
		// An unqualified call that is not a member call or a declarator (a member of this is one too) of a name the file declares
		// a function of above the template (a library function is not found this way). Whether anything of that
		// name comes later is known only at the end of the file.
		for (size_t i = 0; i + 1 < declaration.tokens.size(); ++i) {
			if (declaration.tokens[i].kind != TokenKind::Identifier || declaration.tokens[i + 1].kind != TokenKind::LParen
				|| (i > 0 && declaration.tokens[i - 1].kind == TokenKind::Identifier
					&& !std::set<std::string_view>{ "return", "else", "case", "do", "throw", "sizeof", "new" }.count(declaration.tokens[i - 1].text))
				|| (i > 0 && (declaration.tokens[i - 1].kind == TokenKind::Dot || declaration.tokens[i - 1].kind == TokenKind::Arrow
					|| declaration.tokens[i - 1].kind == TokenKind::ColonColon))) {
				continue;
			}
			const std::string call(declaration.tokens[i].text);
			bool declaredAbove = false;
			for (const std::string& key: declaration.declaredBefore) {
				const auto kind = _declared.find(key);
				declaredAbove = declaredAbove || (kind != _declared.end() && kind->second == "function"
					&& unqualifiedName(key) == call);
			}
			if (declaredAbove) {
				_adlChecks.push_back({ declaration.name, declaration.order, call });
			}
		}
	}
	// Apple stops at 1024.
	if (_classDepth >= 64) {
		throw ClassTemplateError("the instantiation of " + quoted + " nests deeper than 64 levels, at \"" + instance + "\"");
	}

	_spellings->push_back(spelled);
	const std::string_view view = _spellings->back();
	std::vector<Token> tokens = declaration.tokens;
	tokens[1].text = view;
	// The name inside the body means the instance (the injected class name), and is what a constructor is
	// declared by.
	for (size_t i = 2; i + 1 < tokens.size(); ++i) {
		const TokenKind before = tokens[i - 1].kind;
		if (tokens[i].kind == TokenKind::Identifier && tokens[i].text == declaration.spelled
			&& before != TokenKind::Dot && before != TokenKind::Arrow && before != TokenKind::ColonColon
			&& tokens[i + 1].kind != TokenKind::Less) {
			tokens[i].text = view;
		}
	}

	const size_t helpersBefore = _unit.helpers.size();
	const size_t templatesBefore = _unit.templates.size();
	const size_t orderBefore = _functionOrder;
	_classInProgress.insert(instance);
	++_classDepth;
	struct Done {
		Parser& parser;
		const std::string& instance;
		~Done() {
			--parser._classDepth;
			parser._classInProgress.erase(instance);
		}
	} done { *this, instance };
	try {
		const Bindings bindings(*this, parameters);
		const Defined defined(*this, &declaration.declaredBefore);
		const Guard guard(*this, fresh(std::move(tokens)));
		bindParameters(declaration.name, parameters, arguments);
		parseDeclaration();
		if (!at(TokenKind::EndOfFile)) {
			throw CompileError("unexpected \"" + std::string(current().text) + "\" after the template's struct");
		}
	} catch (const CompileError& error) {
		throw ClassTemplateError("in " + quoted + " (declared at line " + std::to_string(declaration.line)
			+ "), instantiated as \"" + spelled + "\": " + error.what());
	}
	_functionOrder = orderBefore;
	if (!_unit.findStruct(instance)) {
		throw ClassTemplateError("the instance \"" + instance + "\" of " + quoted + " did not declare a struct");
	}
	for (StructDecl& made: _unit.structs) {
		if (made.name == instance) {
			made.classTemplate = declaration.name;
			made.classArguments = arguments;
		}
	}
	for (size_t i = helpersBefore; i < _unit.helpers.size(); ++i) {
		// A nested instance's helpers were stamped with their own template's order when it was made.
		if (!_instanceHelpers.insert(i).second) {
			continue;
		}
		FunctionDecl& member = _unit.helpers[i];
		member.isLocal = true;
		member.order = declaration.order;
		member.globalsBefore = declaration.globalsBefore;
	}
	// A member function template of the instance is declared where the class template is, and reads the
	// class template's parameters as the instance bound them.
	for (size_t i = templatesBefore; i < _unit.templates.size(); ++i) {
		FunctionTemplate& member = _unit.templates[i];
		if (member.memberOf == instance) {
			member.order = member.bodyOrder = declaration.order;
			member.globalsBefore = declaration.globalsBefore;
			member.ownerTemplate = _classTemplates.at(declaration.name);
			member.ownerArguments = arguments;
		}
	}
	return instance;
}

// The name in the standard library's namespace that a resolved name denotes, or none when the
// file declares its own: an unqualified name is resolved as spelled, so a local or a declaration
// of that spelling hides the library's.
std::string Parser::libraryName(const std::string& resolved) const {
	if (resolved.rfind("metal::", 0) == 0) {
		return resolved.substr(7);
	}
	if (resolved.find("::") == std::string::npos && kindsOf(resolved).empty() && !isLocal(resolved)
		&& !memberHidesName(resolved)) {
		return resolved;
	}
	return std::string();
}

// "is_same<A, B>::value", "is_same_v<A, B>" and the other one- and two-type predicates, as a bool.
ExpressionPtr Parser::tryParseTraitValue() {
	if (kind() != TokenKind::Identifier && !at(TokenKind::ColonColon)) {
		return nullptr;
	}
	// Only a spelling that ends in "is_...<" is resolved: resolving every identifier here would throw
	// for a function name that is ambiguous until its arguments choose the overload.
	const auto tokenAt = [&](size_t offset) -> const Token& {
		return _tokens[std::min(_position + offset, _tokens.size() - 1)];
	};
	size_t end = at(TokenKind::ColonColon) ? 1 : 0;
	while (tokenAt(end + 1).kind == TokenKind::ColonColon && tokenAt(end + 2).kind == TokenKind::Identifier) {
		end += 2;
	}
	if (tokenAt(end).kind != TokenKind::Identifier || tokenAt(end).text.rfind("is_", 0) != 0
		|| tokenAt(end + 1).kind != TokenKind::Less) {
		return nullptr;
	}
	size_t nameTokens = 0;
	std::string name = peekResolved(nameTokens);
	name = libraryName(name);
	if (name.rfind("is_", 0) != 0 || lookahead(nameTokens).kind != TokenKind::Less) {
		return nullptr;
	}
	const bool variable = name.size() > 2 && name.compare(name.size() - 2, 2, "_v") == 0;
	const std::string trait = variable ? name.substr(0, name.size() - 2) : name;
	static const std::set<std::string> kTraits = { "is_same", "is_floating_point", "is_integral", "is_arithmetic",
		"is_signed", "is_unsigned", "is_scalar", "is_vector", "is_class", "is_enum", "is_const" };
	if (!kTraits.count(trait)) {
		throw CompileError("the trait \"" + trait + "\" is not lowered; mslc folds is_same, is_floating_point, "
			"is_integral, is_arithmetic, is_signed, is_unsigned, is_scalar, is_vector, is_class, is_enum and is_const");
	}
	const size_t startLine = line();
	for (size_t i = 0; i < nameTokens; ++i) {
		advance();
	}
	advance();
	std::vector<Type> operands;
	const size_t wanted = trait == "is_same" ? 2 : 1;
	for (;;) {
		operands.push_back(parseType());
		if (operands.size() < wanted) {
			expect(TokenKind::Comma, ("between the types of \"" + trait + "\"").c_str());
			continue;
		}
		break;
	}
	closeAngle(("to close the types of \"" + trait + "\"").c_str());
	if (!variable) {
		expect(TokenKind::ColonColon, ("after \"" + trait + "<...>\"").c_str());
		if (!atKeyword("value")) {
			throw CompileError("expected \"value\" after \"" + trait + "<...>::\"");
		}
		advance();
	}
	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::BoolLiteral;
	expression->line = startLine;
	try {
		expression->boolValue = traitHolds(trait, operands, _unit);
	} catch (const DependentTrait&) {
		if (!_inEnableIf) {
			throw;
		}
		_dependentCondition = true;
		expression->boolValue = true;
	}
	return expression;
}

// static_assert(condition) or static_assert(condition, "message"), the keyword consumed ([dcl.pre]): the condition is
// an integral constant expression, checked where it is read. A function template's body is read once per instance,
// with its arguments bound, so each instance checks its own; a false one is an error naming the message.
// In a statement (inStatement), two checks are left to the statement's lowering, and returned as the error that
// lowering it is: one in a member function of a class template's instance, whose body is instantiated only where
// the function is used ([temp.inst]) although mslc reads it with the class, and a condition the evaluator cannot
// fold, so a function never lowered is not refused for it.
std::string Parser::parseStaticAssert(bool inStatement) {
	const size_t assertLine = line();
	expect(TokenKind::LParen, "after \"static_assert\"");
	const ExpressionPtr condition = parseConditional();
	std::string message;
	if (match(TokenKind::Comma)) {
		if (!at(TokenKind::StringLiteral)) {
			throw CompileError("the message of a static_assert has to be a string literal");
		}
		// Adjacent literals are one string ([lex.string]).
		while (at(TokenKind::StringLiteral)) {
			std::string_view text = current().text;
			if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
				text = text.substr(1, text.size() - 2);
			}
			// The simple escapes are read as the characters they stand for ([lex.ccon]); any other is kept as spelled.
			for (size_t i = 0; i < text.size(); ++i) {
				if (text[i] != '\\' || i + 1 == text.size()) {
					message += text[i];
					continue;
				}
				const char escaped = text[++i];
				switch (escaped) {
					case 'n': message += '\n'; break;
					case 't': message += '\t'; break;
					case '"': case '\'': case '\\': case '?': message += escaped; break;
					default: message += '\\'; message += escaped; break;
				}
			}
			advance();
		}
	}
	std::string failure;
	try {
		if (evaluateConstant(*condition, false) == 0) {
			failure = "static assertion failed" + (message.empty() ? std::string() : ": " + message)
				+ " (line " + std::to_string(assertLine) + ")";
		}
	} catch (const CompileError& error) {
		// Only the evaluator's own refusal: a subclass carries something its catcher acts on.
		if (!inStatement || typeid(error) != typeid(CompileError)) {
			throw;
		}
		failure = error.what();
	}
	const bool deferred = inStatement && (!_classInProgress.empty() || failure.rfind("static assertion failed", 0) != 0);
	if (!failure.empty() && !deferred) {
		throw CompileError(failure);
	}
	expect(TokenKind::RParen, "to close \"static_assert(\"");
	expect(TokenKind::Semicolon, "after \"static_assert(...)\"");
	return failure;
}

// enable_if, enable_if_t, make_scalar and make_scalar_t, the name consumed and "<" next.
void Parser::parseTraitType(const std::string& name, Type& type) {
	const bool alias = name.size() > 2 && name.compare(name.size() - 2, 2, "_t") == 0;
	const bool enableIf = name.rfind("enable_if", 0) == 0;
	expect(TokenKind::Less, ("after \"" + name + "\"").c_str());
	Type result;
	if (enableIf) {
		bool holds = false;
		{
			const RestoreFlag noGreater(_noGreater, true);
			const RestoreFlag inside(_inEnableIf, true);
			const RestoreFlag dependent(_dependentCondition, false);
			const ExpressionPtr condition = parseConditional();
			// A condition that names a type parameter being deduced holds for now: the instance's own
			// signature is read again with the type, and that read decides.
			holds = _dependentCondition || evaluateConstant(*condition, false) != 0;
		}
		result.scalar = ScalarKind::Void;
		if (match(TokenKind::Comma)) {
			result = parseType();
		}
		closeAngle(("to close \"" + name + "<\"").c_str());
		if (!holds) {
			throw SubstitutionFailure("enable_if is not satisfied");
		}
	} else {
		const Type operand = parseType();
		closeAngle(("to close \"" + name + "<\"").c_str());
		if (isDeductionMarker(operand)) {
			if (!_inEnableIf) {
				throw DependentTrait("make_scalar depends on a type that is not deduced yet");
			}
			type = operand;
			if (!alias) {
				expect(TokenKind::ColonColon, ("after \"" + name + "<...>\"").c_str());
				if (!atKeyword("type")) {
					throw CompileError("expected \"type\" after \"" + name + "<...>::\"");
				}
				advance();
			}
			return;
		}
		if (operand.isPointer || operand.arrayLength || operand.resource != ResourceKind::None || operand.isAtomic
			|| operand.addressSpace != AddressSpace::None) {
			throw CompileError("make_scalar of a pointer, array, texture, sampler, atomic or address-space-qualified "
				"type is not lowered");
		}
		result = operand;
		result.isConst = false;
		if (result.matrixColumns == 0 && result.namedType.empty() && result.enumName.empty()) {
			result.vectorWidth = 0;
			result.isPacked = false;
		}
	}
	if (!alias) {
		expect(TokenKind::ColonColon, ("after \"" + name + "<...>\"").c_str());
		if (!atKeyword("type")) {
			throw CompileError("expected \"type\" after \"" + name + "<...>::\"");
		}
		advance();
	}
	const Type qualifiers = type;
	type = result;
	type.isConst = type.isConst || qualifiers.isConst;
	type.isConstexpr = qualifiers.isConstexpr;
	type.addressSpace = qualifiers.addressSpace;
}

} // namespace mslc
