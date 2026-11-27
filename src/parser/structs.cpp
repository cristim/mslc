#include "parser/internal.h"

namespace mslc {

StructDecl Parser::parseStructDeclaration() {
	StructDecl decl;

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a struct name, found " + std::string(tokenKindName(kind())));
	}

	decl.name = qualify(std::string(advance().text));
	parseStructBody(decl);
	match(TokenKind::Semicolon);

	return decl;
}

// From the base list to the closing brace. A typedef reads the declarator after
// that brace, so the semicolon is the caller's.
void Parser::parseStructBody(StructDecl& decl) {
	const StructDecl* const enclosing = _structBody;
	_structBody = &decl;
	struct Restore {
		const StructDecl*& slot;
		const StructDecl* value;
		~Restore() { slot = value; }
	} restore { _structBody, enclosing };
	if (at(TokenKind::Colon)) {
		// Inheritance is not part of the subset; consume the base list so the
		// error names something useful rather than a stray colon.
		advance();
		if (!atKeyword("public") && !atKeyword("private") && !atKeyword("protected")) {
			throw CompileError("struct inheritance is not supported (in \"" + decl.name + "\")");
		}
		advance();
		advance();
	}

	expect(TokenKind::LBrace, "to open a struct body");

	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		if (parseStructFunction(decl)) {
			continue;
		}

		// "float x, y;" : each declarator after the first starts from the type
		// before the first one's stars, and has its own attributes.
		Type base;
		const Type shared = parseType(false, false, &base);
		for (bool first = true;; first = false) {
			StructField field;
			field.type = shared;
			if (!first) {
				field.type = base;
				parseDeclaratorStars(field.type);
			}
			if (field.type.isAtomic && !field.type.isPointer) {
				throw CompileError("an atomic member of a struct (\"" + decl.name + "\") is not lowered yet; "
					"mslc lowers a device atomic_uint or atomic_int pointer parameter");
			}
			expectFieldName(field.name);
			if (field.type.isStatic) {
				throw CompileError("a static data member is not supported (\"" + field.name + "\" in \""
					+ decl.name + "\")");
			}
			if (at(TokenKind::LParen)) {
				throw CompileError("a member function is not lowered yet (\"" + field.name + "\" in \"" + decl.name + "\")");
			}
			if (field.name == "operator") {
				throw CompileError("an operator overload is not lowered (in \"" + decl.name + "\")");
			}
			if (field.type.isPointer && (!first || at(TokenKind::Comma))) {
				throw CompileError("a pointer member (\"" + field.name + "\" in \"" + decl.name + "\") declared "
					"with several declarators is not lowered yet; declare each on its own line");
			}

			// The brackets before the name, as in "float[2] a;", are not C++, and
			// would otherwise be joined or dropped by the suffix read below.
			if (field.type.arrayLength && !field.type.isStdArray) {
				throw CompileError("the array member \"" + field.name + "\" of \"" + decl.name
					+ "\" puts its length before the name; write the length after it, as in float a[2]");
			}
			if (field.type.isStdArray && !field.type.isPointer && at(TokenKind::LBracket)
				&& lookahead().kind != TokenKind::LBracket) {
				throw CompileError("the member \"" + field.name + "\" of \"" + decl.name
					+ "\" is an array of array<T, N>, which is not lowered");
			}
			if (field.type.isStdArray && field.type.isPointer) {
				throw CompileError("the member \"" + field.name + "\" of \"" + decl.name
					+ "\" is a pointer to array<T, N>, which is not lowered");
			}
			if (field.type.isStdArray ? false : parseArrayDimensions(field.type)) {
				throw CompileError("the array member \"" + field.name + "\" of \"" + decl.name
					+ "\" needs a length: a member cannot be an array of unknown size");
			}

			// An attribute list and an array suffix both open with a bracket, but an
			// attribute list opens with two, which is what tells them apart.
			if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
				field.attributes = parseFieldAttributes();
			}
			decl.fields.push_back(std::move(field));
			if (!match(TokenKind::Comma)) {
				break;
			}
		}

		match(TokenKind::Semicolon);
	}

	expect(TokenKind::RBrace, "to close a struct body");
}

// "template <...> R f(...) const {...}" in a struct body. Like a member function it is read once the struct
// is complete, but as a function template: its tokens are kept, and a call that selects it has it read with
// its arguments bound, the object being a first parameter called this (see parseInstance).
void Parser::parseMemberTemplate(StructDecl& decl) {
	const std::string quoted = "\"" + decl.name + "\"";
	if (unqualifiedName(decl.name).empty()) {
		throw CompileError("a member function template of an unnamed struct is not lowered");
	}
	advance();
	if (!at(TokenKind::Less)) {
		throw CompileError("an explicit instantiation of a member template is not lowered (in " + quoted + ")");
	}
	if (lookahead().kind == TokenKind::Greater) {
		throw CompileError("an explicit specialization of a member template is not lowered (in " + quoted + ")");
	}
	FunctionTemplate declaration;
	declaration.line = line();
	const size_t headerStart = _position;
	parseTemplateHeader(declaration);
	if (atKeyword("template") || atKeyword("using") || atKeyword("struct") || atKeyword("class")
		|| atKeyword("union") || atKeyword("typedef") || atKeyword("enum")) {
		throw CompileError("a member template that is not a function is not lowered (in " + quoted + ")");
	}
	size_t ahead = 0;
	for (;;) {
		if (lookahead(ahead).kind == TokenKind::LBracket && lookahead(ahead + 1).kind == TokenKind::LBracket) {
			int depth = 0;
			while (lookahead(ahead).kind != TokenKind::EndOfFile) {
				if (lookahead(ahead).kind == TokenKind::LBracket) {
					++depth;
				} else if (lookahead(ahead).kind == TokenKind::RBracket && --depth == 0) {
					++ahead;
					break;
				}
				++ahead;
			}
			continue;
		}
		if (lookahead(ahead).kind == TokenKind::Identifier && lookahead(ahead).text == "__attribute__"
			&& lookahead(ahead + 1).kind == TokenKind::LParen) {
			++ahead;
			int depth = 0;
			while (lookahead(ahead).kind != TokenKind::EndOfFile) {
				if (lookahead(ahead).kind == TokenKind::LParen) {
					++depth;
				} else if (lookahead(ahead).kind == TokenKind::RParen && --depth == 0) {
					++ahead;
					break;
				}
				++ahead;
			}
			continue;
		}
		if (lookahead(ahead).kind != TokenKind::Identifier) {
			break;
		}
		const std::string_view word = lookahead(ahead).text;
		if (word == "static") {
			declaration.staticMember = true;
			++ahead;
			continue;
		}
		if (word == "explicit" || word == "virtual" || word == "friend") {
			throw CompileError("a member template declared " + std::string(word) + " is not lowered (in " + quoted + ")");
		}
		if (word == "inline" || word == "constexpr") {
			++ahead;
			continue;
		}
		break;
	}
	if (lookahead(ahead).kind == TokenKind::Identifier && lookahead(ahead).text == unqualifiedName(decl.name)
		&& lookahead(ahead + 1).kind == TokenKind::LParen) {
		if (declaration.staticMember) {
			throw CompileError("a constructor template cannot be static (in " + quoted + ")");
		}
		declaration.constructorMember = true;
		decl.hasConstructors = true;
	}
	const std::string name = readTemplateFunction(declaration, headerStart, true);
	if (declaration.constructorMember && !declaration.hasBody) {
		throw CompileError("constructor template \"" + decl.name + "::" + name + "\" is declared and never defined");
	}
	declaration.name = decl.name + "::" + name;
	declaration.memberOf = decl.name;
	declaration.order = declaration.bodyOrder = _functionOrder++;
	declaration.globalsBefore = _unit.globals.size();
	if (declaration.constructorMember) {
		ConstructorInfo& constructors = _constructors[decl.name];
		constructors.helper = declaration.name;
		constructors.hasTemplate = true;
		_unit.functionTemplates.insert(declaration.name);
		_pendingMemberTemplates.push_back(std::move(declaration));
		return;
	}
	if (const auto earlier = _memberFunctions.find(name);
		earlier != _memberFunctions.end() && earlier->second != declaration.staticMember) {
		throw CompileError("\"" + declaration.name + "\" is declared both static and not static, which Apple takes and mslc does not lower");
	}
	_memberFunctions[name] = declaration.staticMember;
	if (declaration.staticMember) {
		_unit.functionTemplates.insert(declaration.name);
	}
	_memberTemplateNames.insert(name);
	_pendingMemberTemplates.push_back(std::move(declaration));
}

// A member that is not a field: a constructor, a member function or a member
// that cannot be lowered. False, with nothing consumed, when the member is a
// field. A function is skipped here and parsed once the struct is complete, since
// its body may name fields declared after it and the struct's own type.
bool Parser::parseStructFunction(StructDecl& decl) {
	const std::string quoted = "\"" + decl.name + "\"";
	if (atKeyword("template")) {
		parseMemberTemplate(decl);
		return true;
	}
	if (at(TokenKind::Tilde)) {
		throw CompileError("a destructor is not supported (in " + quoted + ")");
	}

	const size_t start = _position;
	size_t ahead = 0;
	while (lookahead(ahead).kind == TokenKind::Identifier
		&& (lookahead(ahead).text == "explicit" || lookahead(ahead).text == "inline"
			|| lookahead(ahead).text == "constexpr" || lookahead(ahead).text == "static")) {
		++ahead;
	}

	const std::string unqualified = unqualifiedName(decl.name);
	bool isFunction = !unqualified.empty() && lookahead(ahead).kind == TokenKind::Identifier
		&& lookahead(ahead).text == unqualified && lookahead(ahead + 1).kind == TokenKind::LParen;
	decl.hasConstructors = decl.hasConstructors || isFunction;
	bool isStatic = false;
	for (size_t i = 0; i < ahead; ++i) {
		isStatic = isStatic || lookahead(i).text == "static";
	}
	std::string functionName;
	if (!isFunction) {
		try {
			_position = start + ahead;
			parseType();
			if (at(TokenKind::Ampersand) && lookahead().kind == TokenKind::Identifier && lookahead().text == "operator") {
				throw CompileError("an operator overload returning a reference is not lowered (in " + quoted + ")");
			}
			if (atKeyword("operator")) {
				if (isStatic || lookahead(1).kind != TokenKind::LBracket
					|| lookahead(2).kind != TokenKind::RBracket || lookahead(3).kind != TokenKind::LParen) {
					throw CompileError("an operator overload is not lowered (in " + quoted + ")");
				}
				_position += 3; functionName = "operator[]"; isFunction = true;
			} else if (kind() == TokenKind::Identifier) {
				functionName = std::string(advance().text);
				isFunction = at(TokenKind::LParen);
			}
		} catch (const ClassTemplateError&) {
			throw;
		} catch (const CompileError& error) {
			if (std::string_view(error.what()).find("operator overload") != std::string_view::npos) {
				throw;
			}
		}
		_position = start;
	}
	if (!isFunction) {
		return false;
	}
	if (unqualified.empty()) {
		throw CompileError("a member function or constructor of an unnamed struct is not lowered");
	}

	size_t depth = 0;
	while (!at(TokenKind::EndOfFile)) {
		if (at(TokenKind::LParen)) {
			++depth;
		} else if (at(TokenKind::RParen) && depth > 0) {
			--depth;
		} else if (depth == 0 && at(TokenKind::Semicolon)) {
			advance();
			break;
		} else if (depth == 0 && at(TokenKind::LBrace)) {
			for (size_t open = 0; !at(TokenKind::EndOfFile); advance()) {
				open += at(TokenKind::LBrace);
				open -= at(TokenKind::RBrace);
				if (open == 0) {
					advance();
					break;
				}
			}
			break;
		}
		advance();
	}

	_pendingMembers.push_back(start);
	if (!functionName.empty()) {
		_memberFunctions[functionName] = isStatic;
	}
	return true;
}

std::string Parser::unqualifiedName(const std::string& name) {
	const size_t tail = name.rfind("::");
	return tail == std::string::npos ? name : name.substr(tail + 2);
}

void Parser::finishStructFunctions(const StructDecl& decl) {
	const std::vector<size_t> starts = std::move(_pendingMembers);
	_pendingMembers.clear();
	std::vector<FunctionTemplate> templates = std::move(_pendingMemberTemplates);
	_pendingMemberTemplates.clear();
	if (starts.empty() && templates.empty()) {
		return;
	}

	_memberStruct = decl.name;
	std::set<std::string> fields;
	std::vector<std::string> fieldOrder;
	for (const StructField& field: decl.fields) {
		fields.insert(field.name);
		fieldOrder.push_back(field.name);
	}

	const size_t resume = _position;
	const size_t helpersBefore = _unit.helpers.size();
	_memberFields = &fields;
	for (const size_t start: starts) {
		parseStructMember(decl, start);
	}
	_memberFields = nullptr;
	const size_t structOrder = _functionOrder++;
	for (size_t i = helpersBefore; i < _unit.helpers.size(); ++i) {
		if (!_instanceHelpers.count(i)) {
			_unit.helpers[i].order = structOrder;
		}
	}
	// A member template is read where the struct ends, with every member it can name in hand.
	if (!templates.empty()) {
		const std::shared_ptr<const void> names = captureNames();
		for (FunctionTemplate& member: templates) {
			member.order = member.bodyOrder = structOrder;
			member.names = names;
			member.memberFieldOrder = fieldOrder;
			member.memberFields = fields;
			member.memberFunctions = _memberFunctions;
			member.spellings = _spellings;
			_unit.templates.push_back(std::move(member));
		}
	}
	_memberFunctions.clear();
	_position = resume;
}


// One member function or constructor, lowered to a helper function named
// "Struct::member" in the helper list. A member function takes its object by
// value as a first parameter called this; a constructor takes its own parameters,
// declares a local this, assigns its initialiser list in field order, runs its
// body and returns the object.
void Parser::parseStructMember(const StructDecl& decl, size_t start) {
	_position = start;
	const std::string tag = unqualifiedName(decl.name);
	bool isStatic = false;
	while (atKeyword("explicit") || atKeyword("inline") || atKeyword("constexpr") || atKeyword("static")) {
		isStatic = isStatic || atKeyword("static");
		advance();
	}

	FunctionDecl fn;
	fn.line = line();
	Type self;
	self.namedType = decl.name;
	const bool isConstructor = atKeyword(tag.c_str()) && lookahead().kind == TokenKind::LParen;
	std::string memberName = tag;
	if (isConstructor) {
		fn.returnType = self;
		advance();
	} else {
		fn.returnType = parseType();
		if (atKeyword("operator")) {
			advance();
			expect(TokenKind::LBracket, "after 'operator'");
			expect(TokenKind::RBracket, "after '['");
			memberName = "operator[]";
		} else {
			memberName = std::string(advance().text);
		}
	}
	advance();

	fn.name = decl.name + "::" + memberName;
	const std::string quoted = std::string(isConstructor ? "constructor \"" : "member function \"") + fn.name + "\"";
	rejectUnloweredReturn(fn.returnType, quoted);
	fn.order = _functionOrder++;
	fn.globalsBefore = _unit.globals.size();
	declareName(fn.name, "function");

	const LocalScope parameters(*this);
	if (!isStatic) {
		declareLocal("this");
	}
	if (!isStatic && !isConstructor) {
		Parameter object;
		object.type = self;
		object.name = "this";
		fn.parameters.push_back(std::move(object));
	}
	parseHelperParameters(fn, quoted);
	if (memberName == "operator[]" && fn.parameters.size() != 2) {
		throw CompileError(quoted + " must take exactly one parameter");
	}

	std::vector<std::pair<std::string, ExpressionPtr>> initialisers;
	if (isConstructor && match(TokenKind::Colon)) {
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

	if (!isConstructor && !isStatic) {
		bool sawConst = false;
		AddressSpace space = AddressSpace::None;
		for (; kind() == TokenKind::Identifier; advance()) {
			const std::string_view word = current().text;
			if (word == "const") {
				if (sawConst) { throw CompileError("duplicate 'const' qualifier on " + quoted); }
				sawConst = true;
			} else if (word == "volatile") {
				throw CompileError("a volatile member function is not lowered (" + quoted + ")");
			} else if (const auto found = addressSpaceFor(word)) {
				if (space != AddressSpace::None) { throw CompileError("duplicate address-space qualifier on " + quoted); }
				if (*found != AddressSpace::Thread && *found != AddressSpace::Constant) {
					throw CompileError(quoted + " is qualified " + std::string(word) + "; only thread and constant member functions are lowered");
				}
				space = *found;
			} else { break; }
		}
		if (!sawConst) {
			throw CompileError(quoted + " is not const; mslc takes the object by value, so a member function "
				"that could write it is not lowered (mark it const)");
		}
		fn.parameters[0].type.isConst = true;
		fn.parameters[0].type.addressSpace = space == AddressSpace::Constant ? AddressSpace::Constant : AddressSpace::Thread;
		fn.parameters[0].isReference = space == AddressSpace::Constant;
	}

	ConstructorInfo* constructors = isConstructor ? &_constructors[decl.name] : nullptr;
	if (match(TokenKind::Assign)) {
		if (!isConstructor || !matchIdentifier("default") || fn.parameters.size() != 0) {
			throw CompileError("only a default constructor may be \"= default\" (" + quoted + ")");
		}
		expect(TokenKind::Semicolon, "after \"= default\"");
		constructors->hasDefault = true;
		return;
	}

	if (match(TokenKind::Semicolon)) {
		if (isConstructor) {
			if (!fn.parameters.empty()) {
				throw CompileError(quoted + " is declared and never defined");
			}
			constructors->hasDefault = true;
			return;
		}
		fn.isLocal = true;
		_unit.helpers.push_back(std::move(fn));
		return;
	}

	fn.body = parseCompoundStatement(true);
	fn.isLocal = true;
	if (!isConstructor) {
		_unit.helpers.push_back(std::move(fn));
		return;
	}

	if (fn.parameters.empty() && initialisers.empty() && fn.body->children.empty()) {
		constructors->hasDefault = true;
		return;
	}
	if (constructors->hasNonTemplate) {
		throw CompileError("overloading the constructors of \"" + decl.name + "\" is not lowered yet");
	}
	constructors->hasNonTemplate = true;
	constructors->helper = fn.name;
	constructors->parameters = fn.parameters.size();

	const auto rewriteReturn = [&](const auto& self, Statement& s) -> void {
		if (s.kind == StatementKind::Return) {
			if (s.expression) {
				throw CompileError(quoted + " is a constructor and cannot return a value");
			}
			s.expression = makeIdentifier("this", s.line);
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
	rewriteReturn(rewriteReturn, *fn.body);

	auto wrapper = std::make_unique<Statement>();
	wrapper->kind = StatementKind::Compound;
	wrapper->line = fn.line;
	auto object = std::make_unique<Statement>();
	object->kind = StatementKind::DeclarationStatement;
	object->line = fn.line;
	object->declaration = VariableDeclaration();
	object->declaration->type = self;
	object->declaration->name = "this";
	wrapper->children.push_back(std::move(object));
	for (const StructField& field: decl.fields) {
		for (auto& [name, value]: initialisers) {
			if (name != field.name) {
				continue;
			}
			auto assign = std::make_unique<Expression>();
			assign->kind = ExpressionKind::Assign;
			assign->line = fn.line;
			assign->left = makeThisMember(name, fn.line);
			assign->right = std::move(value);
			measure(*assign);
			auto statement = std::make_unique<Statement>();
			statement->kind = StatementKind::ExpressionStatement;
			statement->line = fn.line;
			statement->expression = std::move(assign);
			wrapper->children.push_back(std::move(statement));
		}
	}
	wrapper->children.push_back(std::move(fn.body));
	auto result = std::make_unique<Statement>();
	result->kind = StatementKind::Return;
	result->line = fn.line;
	result->expression = makeIdentifier("this", fn.line);
	wrapper->children.push_back(std::move(result));
	fn.body = std::move(wrapper);
	_unit.helpers.push_back(std::move(fn));
}

// A construction of a struct that has a constructor with something to do: the
// call of the helper that stands for it. Null for a struct with none, and for a
// default construction that has nothing to do.
ExpressionPtr Parser::constructWithConstructor(const Type& type, std::vector<ExpressionPtr>& arguments,
	size_t atLine) {

	if (type.namedType.empty() || type.isPointer || type.arrayLength) {
		return nullptr;
	}
	const auto found = _constructors.find(type.namedType);
	if (found == _constructors.end()) {
		return nullptr;
	}

	const ConstructorInfo& info = found->second;
	const bool constructs = arguments.empty()
		? (info.parameters == 0 || info.hasTemplate) && !info.helper.empty() && !info.hasDefault
		: !info.helper.empty();
	if (!constructs) {
		if (arguments.empty() && info.hasDefault) {
			return nullptr;
		}
		throw CompileError("no constructor of \"" + type.namedType + "\" takes " + std::to_string(arguments.size())
			+ " arguments");
	}

	auto call = std::make_unique<Expression>();
	call->kind = ExpressionKind::Call;
	call->line = atLine;
	call->left = makeIdentifier(info.helper, atLine);
	call->arguments = std::move(arguments);
	measure(*call);
	return call;
}

} // namespace mslc
