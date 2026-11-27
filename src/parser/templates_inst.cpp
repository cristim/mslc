#include "parser/internal.h"

namespace mslc {

// One instance of a template as a helper. The declaration's tokens are the whole input.
FunctionDecl Parser::parseInstance(const FunctionTemplate& declaration, const std::vector<TemplateArgument>& arguments,
	bool withBody) {
	restoreNames(*namesAt(declaration));
	const LocalScope templateScope(*this);
	_inInstance = true;
	const bool member = !declaration.memberOf.empty();
	if (member) {
		_memberFields = &declaration.memberFields;
		_memberFunctions = declaration.memberFunctions;
		_memberStruct = declaration.memberOf;
		if (const ClassTemplate* owner = declaration.ownerTemplate.get()) {
			_classMemberInstance = true;
			_definedNames = &owner->declaredBefore;
			bindParameters(owner->name, owner->parameters, declaration.ownerArguments);
			for (const TemplateParameter& parameter: declaration.parameters) {
				for (const TemplateParameter& outer: owner->parameters) {
					if (!parameter.name.empty() && parameter.name == outer.name) {
						throw CompileError("the template parameter \"" + parameter.name + "\" of the member template \""
							+ declaration.name + "\" shadows a parameter of its class template");
					}
				}
			}
		}
	}
	bindTemplateArguments(declaration, arguments);

	const auto skipAttributes = [&] {
		while (atKeyword("__attribute__") && lookahead().kind == TokenKind::LParen) {
			skipFunctionAttribute();
		}
	};
	FunctionDecl decl;
	decl.stage = declaration.stage;
	decl.line = declaration.line;
	decl.name = declaration.name;
	decl.order = declaration.bodyOrder;
	decl.globalsBefore = declaration.globalsBefore;
	const std::string quoted = "function template \"" + declaration.name + "\"";

	if (declaration.stage != Stage::None) {
		advance();
	} else {
		for (;;) {
			skipAttributes();
			if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
				if (skipStandardFunctionAttribute(true)) {
					continue;
				}
				break;
			}
			if (atKeyword("inline") || atKeyword("static") || atKeyword("constexpr")) {
				advance();
				continue;
			}
			break;
		}
	}
	if (at(TokenKind::LBracket)) {
		throw CompileError("an attribute in the declaration of " + quoted + " is not lowered yet");
	}
	if (atKeyword("auto")) {
		throw CompileError("a trailing return type on " + quoted + " is not lowered yet");
	}
	const std::string written = declaration.name.substr(declaration.name.rfind("::") == std::string::npos
		? 0 : declaration.name.rfind("::") + 2);
	// The signature read to deduce the parameters, some of which stand for themselves: a return type that is
	// an instance of a class template on those parameters cannot be made yet, and deduction does not need it.
	// It is a dependent type, as a return type of T is; the instance a call chooses reads the real one.
	const bool deducing = !withBody && std::any_of(arguments.begin(), arguments.end(), [](const TemplateArgument& argument) {
		return argument.unbound || (argument.isType && isDeductionMarker(argument.type));
	});
	const size_t returnTypeStart = _position;
	bool dependentReturn = false;
	if (declaration.constructorMember) {
		decl.returnType.namedType = declaration.memberOf;
	} else {
		try {
			decl.returnType = parseType();
		} catch (const DependentClassInstance&) {
			if (!deducing) {
				throw;
			}
			_position = returnTypeStart;
			// A "<" or ">" inside parentheses, "(1 < 2)", is an operator, not a bracket.
			size_t angles = 0;
			size_t parentheses = 0;
			while (!at(TokenKind::EndOfFile) && !(angles == 0 && parentheses == 0 && kind() == TokenKind::Identifier
				&& current().text == written && lookahead().kind == TokenKind::LParen)) {
				parentheses += at(TokenKind::LParen) ? 1 : 0;
				parentheses -= at(TokenKind::RParen) && parentheses > 0 ? 1 : 0;
				if (parentheses == 0) {
					angles += at(TokenKind::Less) ? 1 : 0;
					angles -= at(TokenKind::Greater) && angles > 0 ? 1 : 0;
					angles -= at(TokenKind::ShiftRight) && angles > 1 ? 2 : 0;
				}
				advance();
			}
			if (at(TokenKind::EndOfFile)) {
				throw;
			}
			decl.returnType = Type();
			decl.returnType.namedType = "<dependent return type>";
			dependentReturn = true;
		}
	}
	if (declaration.stage == Stage::None) {
		rejectUnloweredReturn(decl.returnType, quoted);
	}
	if (kind() != TokenKind::Identifier || current().text != written) {
		throw CompileError("the declaration of " + quoted + " is not a return type and a name that mslc reads "
			"when it is instantiated (found \"" + std::string(current().text) + "\" where its name is)");
	}
	advance();
	if (at(TokenKind::LBracket)) {
		throw CompileError("an attribute in the declaration of " + quoted + " is not lowered yet");
	}
	expect(TokenKind::LParen, "to open the parameters of a function template");

	const LocalScope parameters(*this);
	_parameterBlock = _blocks.size() - 1;
	// The object is a first parameter, as in a member function.
	const bool object = member && !declaration.staticMember && !declaration.constructorMember;
	const size_t objectParameters = object ? 1 : 0;
	if (object || declaration.constructorMember) {
		declareLocal("this");
	}
	if (object) {
		Parameter self;
		self.type.namedType = declaration.memberOf;
		self.name = "this";
		decl.parameters.push_back(std::move(self));
	}
	const size_t parametersFrom = _position;
	if (declaration.stage != Stage::None) {
		parseEntryParameterList(decl.parameters, quoted, !withBody && !declaration.hasBody);
		if (withBody) {
			checkUniqueResourceSlots(decl);
		}
	} else {
		parseHelperParameters(decl, quoted);
	}
	// A parameter is dependent when the tokens that declare it name a template parameter.
	{
		size_t index = 0;
		bool dependent = false;
		int depth = 0;
		for (size_t i = parametersFrom; i + 1 < _position; ++i) {
			const Token& token = _tokens[i];
			if (token.kind == TokenKind::LParen || token.kind == TokenKind::LBracket || token.kind == TokenKind::Less) {
				++depth;
			} else if (token.kind == TokenKind::RParen || token.kind == TokenKind::RBracket || token.kind == TokenKind::Greater) {
				--depth;
			}
			if (token.kind == TokenKind::Comma && depth == 0) {
				if (dependent && index + objectParameters < decl.parameters.size()) {
					_dependentParameters.insert(decl.parameters[index + objectParameters].name);
				}
				++index;
				dependent = false;
			} else if (token.kind == TokenKind::Identifier) {
				for (const TemplateParameter& parameter: declaration.parameters) {
					dependent = dependent || (!parameter.name.empty() && parameter.name == token.text);
				}
			}
		}
		if (dependent && index + objectParameters < decl.parameters.size()) {
			_dependentParameters.insert(decl.parameters[index + objectParameters].name);
		}
	}
	if (object && !matchIdentifier("const")) {
		throw CompileError(quoted + " is not const; mslc takes the object by value, so a member function that "
			"could write it is not lowered (mark it const)");
	}
	if (declaration.constructorMember && matchIdentifier("const")) {
		throw CompileError(quoted + " is a constructor and cannot be declared const");
	}
	// A struct the template's own tokens name has to be declared above it; one an argument brings is not
	// looked up here (sema sees every struct, so it cannot tell the two apart).
	std::set<std::string> bound;
	for (const TemplateArgument& argument: arguments) {
		bound.insert(argument.type.namedType);
	}
	const auto declared = [&](const Type& type) {
		if (!type.namedType.empty() && !_unit.findStruct(type.namedType) && !bound.count(type.namedType)
			&& !_dependentInstances.count(type.namedType)) {
			throw CompileError("unknown type name \"" + type.namedType + "\" in the declaration of " + quoted);
		}
	};
	if (!dependentReturn) {
		declared(decl.returnType);
	}
	for (const Parameter& parameter: decl.parameters) {
		declared(parameter.type);
	}
	decl.dependentInstances = _dependentInstances;
	skipAttributes();
	if (at(TokenKind::LBracket)) {
		throw CompileError("attributes on " + quoted + " are not lowered yet");
	}
	if (at(TokenKind::Arrow)) {
		throw CompileError("a trailing return type on " + quoted + " is not lowered yet");
	}
	if (declaration.constructorMember && decl.parameters.empty()) {
		throw CompileError("constructor template \"" + declaration.name + "\" takes no arguments, which is not lowered yet");
	}
	if (!withBody || (!declaration.constructorMember && match(TokenKind::Semicolon))) {
		return decl;
	}
	std::vector<std::pair<std::string, ExpressionPtr>> initialisers;
	if (declaration.constructorMember && match(TokenKind::Colon)) {
		do {
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a member name in the initialiser list of " + quoted);
			}
			const std::string field(advance().text);
			if (!_memberFields->count(field)) {
				throw CompileError("\"" + field + "\" in the initialiser list of " + quoted + " is not a member");
			}
			for (const auto& done: initialisers) {
				if (done.first == field) {
					throw CompileError("the member \"" + field + "\" is initialised twice in " + quoted);
				}
			}
			if (!match(TokenKind::LParen)) {
				throw CompileError("only \"member(value)\" is lowered in the initialiser list of " + quoted);
			}
			ExpressionPtr value = parseAssignment();
			expect(TokenKind::RParen, "to close a member initialiser");
			initialisers.emplace_back(field, std::move(value));
		} while (match(TokenKind::Comma));
	}
	decl.body = parseCompoundStatement(true);
	if (!at(TokenKind::EndOfFile)) {
		throw CompileError("unexpected \"" + std::string(current().text) + "\" after the body of " + quoted);
	}
	if (declaration.constructorMember) {
		const auto rewriteReturn = [&](const auto& self, Statement& s) -> void {
			if (s.kind == StatementKind::Return) {
				if (s.expression) {
					throw CompileError(quoted + " is a constructor and cannot return a value");
				}
				s.expression = std::make_unique<Expression>();
				s.expression->kind = ExpressionKind::Identifier;
				s.expression->line = s.line;
				s.expression->name = "this";
			}
			for (StatementPtr& child: s.children) { self(self, *child); }
			if (s.thenBranch) { self(self, *s.thenBranch); }
			if (s.elseBranch) { self(self, *s.elseBranch); }
			if (s.forBody) { self(self, *s.forBody); }
			if (s.whileBody) { self(self, *s.whileBody); }
			for (SwitchCase& sc: s.switchCases) {
				for (StatementPtr& child: sc.body) { self(self, *child); }
			}
			for (StatementPtr& child: s.switchPreamble) { self(self, *child); }
		};
		rewriteReturn(rewriteReturn, *decl.body);
		auto wrapper = std::make_unique<Statement>();
		wrapper->kind = StatementKind::Compound;
		wrapper->line = decl.line;
		auto selfLocal = std::make_unique<Statement>();
		selfLocal->kind = StatementKind::DeclarationStatement;
		selfLocal->line = decl.line;
		selfLocal->declaration = VariableDeclaration();
		selfLocal->declaration->type = decl.returnType;
		selfLocal->declaration->name = "this";
		wrapper->children.push_back(std::move(selfLocal));
		for (const std::string& fieldName: declaration.memberFieldOrder) {
			for (auto& [name, value]: initialisers) {
				if (name != fieldName) {
					continue;
				}
				auto assign = std::make_unique<Expression>();
				assign->kind = ExpressionKind::Assign;
				assign->line = decl.line;
				assign->left = std::make_unique<Expression>();
				assign->left->kind = ExpressionKind::Member;
				assign->left->line = decl.line;
				assign->left->left = std::make_unique<Expression>();
				assign->left->left->kind = ExpressionKind::Identifier;
				assign->left->left->line = decl.line;
				assign->left->left->name = "this";
				assign->left->memberName = name;
				assign->left->height = 2;
				assign->right = std::move(value);
				measure(*assign);
				auto statement = std::make_unique<Statement>();
				statement->kind = StatementKind::ExpressionStatement;
				statement->line = decl.line;
				statement->expression = std::move(assign);
				wrapper->children.push_back(std::move(statement));
			}
		}
		wrapper->children.push_back(std::move(decl.body));
		auto result = std::make_unique<Statement>();
		result->kind = StatementKind::Return;
		result->line = decl.line;
		result->expression = std::make_unique<Expression>();
		result->expression->kind = ExpressionKind::Identifier;
		result->expression->line = decl.line;
		result->expression->name = "this";
		wrapper->children.push_back(std::move(result));
		decl.body = std::move(wrapper);
	}
	return decl;
}

FunctionDecl instantiateTemplate(const FunctionTemplate& declaration,
	const std::vector<TemplateArgument>& arguments, bool withBody) {
	Parser parser(declaration.tokens);
	try {
		return parser.parseInstance(declaration, arguments, withBody);
	} catch (const SubstitutionFailure& failure) {
		// Only the signature is a substitution: a failure in the body, which a chosen instance reads, is an error.
		if (!withBody) {
			throw;
		}
		throw CompileError("in the function template \"" + declaration.name + "\" (declared at line "
			+ std::to_string(declaration.line) + "), which a call instantiates: " + failure.what());
	} catch (const CompileError& error) {
		throw CompileError("in the function template \"" + declaration.name + "\" (declared at line "
			+ std::to_string(declaration.line) + "), which a call instantiates: " + error.what());
	}
}

TemplateArgument templateDefault(const FunctionTemplate& declaration, size_t index,
	const std::vector<TemplateArgument>& bound) {
	const TemplateParameter& parameter = declaration.parameters.at(index);
	const std::string what = "the default argument of the template parameter \"" + parameter.name + "\" of \""
		+ declaration.name + "\"";
	Parser parser(parameter.defaultTokens);
	parser.restoreNames(*Parser::namesAt(declaration));
	const Parser::LocalScope scope(parser);
	if (const ClassTemplate* owner = declaration.ownerTemplate.get()) {
		parser._definedNames = &owner->declaredBefore;
		parser.bindParameters(owner->name, owner->parameters, declaration.ownerArguments);
	}
	// Only the parameters before this one are in scope.
	FunctionTemplate earlier;
	earlier.name = declaration.name;
	earlier.parameters.assign(declaration.parameters.begin(), declaration.parameters.begin() + index);
	parser.bindTemplateArguments(earlier, std::vector<TemplateArgument>(bound.begin(), bound.begin() + index));
	TemplateArgument result;
	result.isType = parameter.isType;
	try {
		if (parameter.isType) {
			result.type = parser.parseType();
			if (result.type.isPointer || result.type.arrayLength || result.type.resource != ResourceKind::None
				|| !result.type.enumName.empty() || result.type.addressSpace != AddressSpace::None
				|| result.type.isVolatile) {
				throw CompileError("that is a pointer, an array, a resource, an enum, volatile or in an address space");
			}
		} else {
			const ExpressionPtr expression = parser.parseAssignment();
			result.value = expression->kind == ExpressionKind::BoolLiteral ? (expression->boolValue ? 1 : 0)
				: parser.evaluateConstant(*expression, false);
		}
		if (!parser.at(TokenKind::EndOfFile)) {
			throw CompileError("unexpected \"" + std::string(parser.current().text) + "\"");
		}
	} catch (const SubstitutionFailure&) {
		throw;
	} catch (const CompileError& error) {
		throw CompileError(what + " is not lowered: " + error.what());
	}
	return result;
}

// One explicit template argument: a type, or an integer constant that stops at the "," or ">"
// that ends it (a larger expression needs parentheses, as in C++).
TemplateArgument Parser::parseTemplateArgument() {
	TemplateArgument argument;
	Type probe;
	size_t nameTokens = 0;
	const bool named = kind() == TokenKind::Identifier || at(TokenKind::ColonColon);
	const std::string name = named ? peekResolved(nameTokens) : std::string();
	const bool localHides = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon && isLocal(name);
	size_t vecTokens = 0;
	const bool isTypeArgument = (named && (resolveTypeName(name, probe) || isTypeQualifier(name)
			|| (_unit.findStruct(name) != nullptr && !localHides)))
		|| (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text)) || atLibraryVec(vecTokens);
	if (isTypeArgument) {
		argument.type = parseType();
		if (argument.type.isPointer || argument.type.arrayLength || argument.type.resource != ResourceKind::None
			|| !argument.type.enumName.empty() || argument.type.addressSpace != AddressSpace::None
			|| argument.type.isVolatile) {
			throw CompileError("a template argument that is a pointer, an array, a resource, an enum, volatile or "
				"in an address space is not lowered");
		}
		return argument;
	}
	argument.isType = false;
	const ExpressionPtr value = parseAdditive();
	if (!at(TokenKind::Comma) && !at(TokenKind::Greater)) {
		throw CompileError("this template argument is not lowered: a constant that is more than an arithmetic "
			"expression needs parentheses");
	}
	argument.value = value->kind == ExpressionKind::BoolLiteral ? (value->boolValue ? 1 : 0)
		: evaluateConstant(*value, false);
	return argument;
}

bool isDeductionMarker(const Type& type) {
	return type.namedType.size() > 2 && type.namedType.front() == '<' && type.namedType.back() == '>';
}

bool isScalarOrVector(const Type& type) {
	return type.matrixColumns == 0 && type.namedType.empty() && type.enumName.empty() && type.scalar != ScalarKind::Void;
}

bool sameTraitType(const Type& a, const Type& b) {
	return a.scalar == b.scalar && a.vectorWidth == b.vectorWidth && a.matrixColumns == b.matrixColumns
		&& a.isPacked == b.isPacked && a.namedType == b.namedType && a.enumName == b.enumName && a.isConst == b.isConst;
}


// <type_traits> as Metal's library has it (measured against Apple's compiler), for the types mslc
// has: scalar, vector (packed or not), matrix, struct and enum. A vector counts as floating point,
// integral, signed or unsigned by its element, and as arithmetic; a matrix is a class; an enum is
// a scalar and nothing else numeric. A type with a pointer, array, address space or resource is
// refused, not guessed at.
bool traitHolds(const std::string& trait, const std::vector<Type>& operands, const TranslationUnit& unit) {
	for (const Type& type: operands) {
		if (type.isVolatile) {
			throw CompileError("the trait \"" + trait + "\" of a volatile type is not lowered");
		}
		if (isDeductionMarker(type)) {
			throw DependentTrait("the trait \"" + trait + "\" depends on a type that is not deduced yet");
		}
		if (type.isPointer || type.arrayLength || type.resource != ResourceKind::None || type.isAtomic
			|| type.addressSpace != AddressSpace::None || type.isAuto) {
			throw CompileError("the trait \"" + trait + "\" of a pointer, array, texture, sampler, atomic or "
				"address-space-qualified type is not lowered");
		}
		if (!type.namedType.empty() && !unit.findStruct(type.namedType)) {
			throw CompileError("the trait \"" + trait + "\" of \"" + type.namedType + "\", which is not a type "
				"mslc knows");
		}
		if (type.scalar == ScalarKind::Double && isScalarOrVector(type)) {
			throw CompileError("the trait \"" + trait + "\" of a double is not lowered");
		}
	}
	if (trait == "is_same") {
		return sameTraitType(operands.at(0), operands.at(1));
	}
	const Type& type = operands.at(0);
	const bool numeric = isScalarOrVector(type);
	// Apple (measured): a const vector is neither signed nor unsigned, though it is still a vector,
	// floating point or integral and arithmetic.
	const bool constVector = numeric && type.isConst && type.vectorWidth != 0;
	const ScalarKind kind = type.scalar;
	const bool floating = kind == ScalarKind::Half || kind == ScalarKind::Float;
	const bool unsignedKind = kind == ScalarKind::Bool || kind == ScalarKind::UChar || kind == ScalarKind::UShort
		|| kind == ScalarKind::UInt || kind == ScalarKind::ULong;
	if (trait == "is_floating_point") {
		return numeric && floating;
	}
	if (trait == "is_integral") {
		return numeric && !floating;
	}
	if (trait == "is_arithmetic") {
		return numeric;
	}
	if (trait == "is_signed") {
		return numeric && !unsignedKind && !constVector;
	}
	if (trait == "is_unsigned") {
		return numeric && unsignedKind && !constVector;
	}
	if (trait == "is_scalar") {
		return (numeric && type.vectorWidth == 0) || !type.enumName.empty();
	}
	if (trait == "is_vector") {
		return numeric && type.vectorWidth != 0;
	}
	if (trait == "is_class") {
		return type.matrixColumns != 0 || !type.namedType.empty();
	}
	if (trait == "is_enum") {
		return !type.enumName.empty();
	}
	if (trait == "is_const") {
		return type.isConst;
	}
	throw CompileError("the trait \"" + trait + "\" is not lowered");
}

// The ">" that closes a template argument list; a ">>" is two of them, and the first is taken.
void Parser::closeAngle(const char* what) {
	if (at(TokenKind::ShiftRight)) {
		_tokens[_position].kind = TokenKind::Greater;
		_tokens[_position].text = ">";
		_tokens.insert(_tokens.begin() + _position, _tokens[_position]);
	}
	expect(TokenKind::Greater, what);
}

void Parser::reserveSplits(std::vector<Token>& tokens) {
	tokens.reserve(tokens.size() + std::count_if(tokens.begin(), tokens.end(),
		[](const Token& token) { return token.kind == TokenKind::ShiftRight; }));
}

size_t Parser::usingCount(const std::vector<std::string>& path, const std::set<std::string>& mentioned,
	const std::vector<std::string>& aliasTargets) const {
	size_t count = 0;
	std::vector<std::string> work;
	std::set<std::string> seen;
	std::string scope;
	for (size_t i = 0; i <= path.size(); ++i) {
		if (i > 0) {
			scope += (scope.empty() ? "" : "::") + path[i - 1];
		}
		work.push_back(scope);
	}
	// A qualified call "N::h(v)" reads N, which may be written anywhere: a namespace the template names is a scope
	// it reads from, so a using declaration made in it later is counted (and refused) too.
	for (const auto& scope: _scopes) {
		const std::string& key = scope.first;
		const size_t cut = key.rfind("::");
		if (!key.empty() && mentioned.count(cut == std::string::npos ? key : key.substr(cut + 2))) {
			work.push_back(key);
		}
	}
	work.insert(work.end(), aliasTargets.begin(), aliasTargets.end());
	// The scopes the template's own chain reads from, and every one those nominate by a directive, transitively.
	while (!work.empty()) {
		const std::string current = work.back();
		work.pop_back();
		if (!seen.insert(current).second) {
			continue;
		}
		const auto entry = _scopes.find(current);
		if (entry == _scopes.end()) {
			continue;
		}
		for (const std::string& directive: entry->second.directives) {
			bool own = false;
			std::string prefix;
			for (size_t n = 0; n < path.size() && !own; ++n) {
				prefix += (prefix.empty() ? "" : "::") + path[n];
				own = directive == prefix;
			}
			count += own ? 0 : 1;
			work.push_back(directive);
			// A directive may be written relative to the scope it is in.
			work.push_back(current.empty() ? directive : current + "::" + directive);
		}
		for (const std::string& inner: entry->second.inlines) {
			work.push_back(inner);
		}
		for (const auto& declaration: entry->second.stdlibDeclarations) {
			count += mentioned.count(declaration.first) ? declaration.second.size() : 0;
		}
		for (const auto& declaration: entry->second.declarations) {
			count += mentioned.count(declaration.first) ? declaration.second.size() : 0;
		}
	}
	return count;
}

size_t Parser::usingDeclarationsOf(const std::string& name) const {
	size_t count = 0;
	for (const auto& scope: _scopes) {
		if (const auto found = scope.second.declarations.find(name); found != scope.second.declarations.end()) {
			count += found->second.size();
		}
		if (const auto found = scope.second.stdlibDeclarations.find(name); found != scope.second.stdlibDeclarations.end()) {
			count += found->second.size();
		}
	}
	return count;
}

void Parser::swapContext(Context& other) {
	std::swap(_tokens, other.tokens);
	std::swap(_position, other.position);
	std::swap(_blocks, other.blocks);
	std::swap(_structBody, other.structBody);
	std::swap(_pendingMembers, other.pendingMembers);
	std::swap(_pendingMemberTemplates, other.pendingMemberTemplates);
	std::swap(_memberFields, other.memberFields);
	std::swap(_memberFunctions, other.memberFunctions);
	std::swap(_memberStruct, other.memberStruct);
	std::swap(_parameterBlock, other.parameterBlock);
	std::swap(_dependentParameters, other.dependentParameters);
	std::swap(_switchDepth, other.switchDepth);
	std::swap(_declaratorTail, other.declaratorTail);
	std::swap(_noGreater, other.noGreater);
	std::swap(_inEnableIf, other.inEnableIf);
	std::swap(_dependentCondition, other.dependentCondition);
	std::swap(_stopAt, other.stopAt);
	std::swap(_namespacePath, other.namespacePath);
}

} // namespace mslc
