#include "parser/internal.h"

namespace mslc {

const Token& Parser::lookahead(size_t offset) const {
	const size_t index = _position + offset;
	return index < _tokens.size() ? _tokens[index] : _tokens.back();
}

bool Parser::isDiscardFragment(QualifiedName& name) const {
	if (!peekQualifiedName(name) || name.parts.back() != "discard_fragment"
		|| lookahead(name.tokens).kind != TokenKind::LParen
		|| lookahead(name.tokens + 1).kind != TokenKind::RParen) {
		return false;
	}
	return resolveName(name) == "metal::discard_fragment";
}

const Token& Parser::advance() {
	const Token& token = _tokens[_position];
	if (_position + 1 < _tokens.size()) {
		++_position;
	}
	return token;
}

void Parser::expect(TokenKind expected, const char* context) {
	if (!at(expected)) {
		throw CompileError(std::string("expected ") + tokenKindName(expected) + " " + context
			+ ", found " + tokenKindName(kind()) + " \"" + std::string(current().text) + "\"");
	}

	advance();
}

bool Parser::match(TokenKind expected) {
	if (!at(expected)) {
		return false;
	}

	advance();
	return true;
}

bool Parser::atKeyword(const char* text) const {
	return kind() == TokenKind::Identifier && current().text == text;
}

bool Parser::matchIdentifier(const char* text) {
	if (!atKeyword(text)) {
		return false;
	}

	advance();
	return true;
}

const Token& Parser::expectKeyword(const char* text, const char* context) {
	if (!atKeyword(text)) {
		throw CompileError(std::string("expected \"") + text + "\" " + context
			+ ", found \"" + std::string(current().text) + "\"");
	}

	return advance();
}

size_t Parser::line() const {
	return current().line;
}

std::shared_ptr<const Parser::Names> Parser::namesAt(const FunctionTemplate& declaration) {
	if (!declaration.names) {
		Parser replay(*declaration.file);
		replay._stopAt = declaration.endAt;
		if (declaration.seeds) {
			replay.setSeeds(*declaration.seeds);
		}
		try {
			while (!replay.at(TokenKind::EndOfFile)) {
				replay.parseTopLevel();
			}
		} catch (const StopParsing&) {
		}
		declaration.names = replay.captureNames();
	}
	return std::static_pointer_cast<const Names>(declaration.names);
}

void Parser::setSeeds(std::vector<ClassInstanceSeed> seeds) {
	_seeds = std::move(seeds);
	_seeded.assign(_seeds.size(), false);
	_seedKeys.clear();
	for (const ClassInstanceSeed& seed: _seeds) {
		_seedKeys.insert(seed.instance);
	}
	_seedList = std::make_shared<const std::vector<ClassInstanceSeed>>(_seeds);
}

TranslationUnit Parser::parse() {
	while (!at(TokenKind::EndOfFile)) {
		parseTopLevel();
	}

	// clang makes an instance at the end of the file, and an unqualified call in it finds, by argument-dependent
	// lookup, what is declared below the template. Which argument types reach the call is not modelled, so a
	// later function, function template or using declaration of the name refuses the instance (false refusals
	// for an unrelated function of that name in another namespace are the cost).
	// A seeded instance was made where its class template and arguments first exist, which is above the code that
	// needs it, so a name declared between the two is not seen by the lookup that refuses a late name. clang reads
	// names from the class template's definition, so a file-scope name declared after the template that the
	// template spells (as other than a member, or one of its parameters) is refused whenever it is declared.
	for (size_t i = 0; i < _seeds.size(); ++i) {
		if (!_seeded[i]) {
			continue;
		}
		const ClassTemplate& declaration = *_classTemplates.at(_seeds[i].className);
		if (usingCount(declaration.namespacePath, declaration.mentioned, declaration.aliasTargets) != declaration.usingsBefore) {
			throw CompileError("a using declaration or directive is made after the class template \"" + declaration.name
				+ "\" in a scope it reads names from, and a function template names its instance \""
				+ _seeds[i].instance + "\"; write it above the template");
		}
		std::set<std::string> spelled;
		for (size_t t = 0; t < declaration.tokens.size(); ++t) {
			if (declaration.tokens[t].kind == TokenKind::Identifier
				&& (t == 0 || (declaration.tokens[t - 1].kind != TokenKind::Dot && declaration.tokens[t - 1].kind != TokenKind::Arrow))) {
				spelled.insert(std::string(declaration.tokens[t].text));
			}
		}
		for (const TemplateParameter& parameter: declaration.parameters) {
			spelled.erase(parameter.name);
		}
		for (const auto& [key, kind]: _declared) {
			if (kind != "function" && key.find('<') == std::string::npos && !declaration.declaredBefore.count(key)
				&& spelled.count(unqualifiedName(key))) {
				throw CompileError("the name \"" + key + "\" is declared after the class template \""
					+ declaration.name + "\", whose instance \"" + _seeds[i].instance + "\" a function template "
					"names, and the template spells that name; declare it above the template");
			}
		}
	}
	for (const AdlCheck& check: _adlChecks) {
		const auto refuse = [&](const std::string& what) {
			throw CompileError("the " + what + " \"" + check.call + "\" is declared after the class template \""
				+ check.templateName + "\", which calls \"" + check.call + "\": clang finds it by argument-dependent "
				"lookup where the instance is made (at the end of the file), which mslc does not model; declare it "
				"above the template");
		};
		for (const FunctionDecl& later: _unit.helpers) {
			if (later.order > check.order && later.name.find('<') == std::string::npos
				&& unqualifiedName(later.name) == check.call) {
				refuse("function");
			}
		}
		for (const FunctionTemplate& later: _unit.templates) {
			if (later.order > check.order && unqualifiedName(later.name) == check.call) {
				refuse("function template");
			}
		}
		if (usingDeclarationsOf(check.call) > _classTemplates.at(check.templateName)->usingDeclarationsBefore.at(check.call)) {
			refuse("using declaration of");
		}
	}
	if (!_unit.templates.empty() || !_unit.entryTemplates.empty() || !_unit.specializations.empty()) {
		const auto file = std::make_shared<const std::vector<Token>>(_tokens);
		for (FunctionTemplate& declaration: _unit.templates) {
			declaration.file = file;
			declaration.seeds = _seedList;
		}
		for (FunctionTemplate& declaration: _unit.entryTemplates) {
			declaration.file = file;
			declaration.seeds = _seedList;
		}
		for (ExplicitSpecialization& specialization: _unit.specializations) {
			specialization.declaration.file = file;
			specialization.declaration.seeds = _seedList;
		}
	}
	resolveEntryInstantiations();
	return std::move(_unit);
}

void Parser::parseDeclaration() {
	if (at(TokenKind::Semicolon)) {
		advance();
		return;
	}

	if (matchIdentifier("using")) {
		parseUsing();
		return;
	}

	if (matchIdentifier("static_assert")) {
		parseStaticAssert(false);
		return;
	}

	if (matchIdentifier("namespace")) {
		if (kind() == TokenKind::Identifier && lookahead().kind == TokenKind::Assign) {
			parseNamespaceAlias();
		} else {
			parseNamespace();
		}
		return;
	}

	if (atKeyword("inline") && lookahead().kind == TokenKind::Identifier && lookahead().text == "namespace") {
		advance();
		advance();
		parseNamespace(true);
		return;
	}

	if (matchIdentifier("struct")) {
		StructDecl decl = parseStructDeclaration();
		declareName(decl.name, "struct");
		_unit.structs.push_back(std::move(decl));
		finishStructFunctions(_unit.structs.back());
		return;
	}

	if (atKeyword("kernel")) {
		advance();
		_unit.functions.push_back(parseFunctionDeclaration(Stage::Kernel));
		return;
	}

	if (atKeyword("vertex")) {
		advance();
		_unit.functions.push_back(parseFunctionDeclaration(Stage::Vertex));
		return;
	}

	if (atKeyword("fragment")) {
		advance();
		_unit.functions.push_back(parseFunctionDeclaration(Stage::Fragment));
		return;
	}

	if (matchIdentifier("typedef")) {
		parseTypedef();
		return;
	}

	if (atKeyword("enum")) {
		bool hadBody = false;
		parseEnumDeclaration(hadBody);
		if (!hadBody) {
			throw CompileError("a bare enum declaration without enumerators is not supported");
		}
		expect(TokenKind::Semicolon, "after an enum declaration; a variable declared with it is not supported");
		return;
	}

	if (atKeyword("template")) {
		parseTemplateDeclaration();
		if (_position == _stopAt) {
			throw StopParsing();
		}
		return;
	}

	if (tryParseHelperFunction()) {
		return;
	}

	// A file-scope constant. An address space or a type qualifier ahead of the
	// type is what marks one, since a bare "float kX" at file scope is not valid
	// MSL and would otherwise be read as the start of a function's return type.
	if (libraryResourcePrefixTokens() || (at(TokenKind::Identifier)
		&& (isTypeQualifier(current().text) || addressSpaceFor(current().text)
			|| isResourceTypeName(current().text)
			|| (current().text == "metal" && lookahead().kind == TokenKind::ColonColon
				&& lookahead(2).kind == TokenKind::Identifier && isResourceTypeName(lookahead(2).text))))) {
		parseGlobalDeclaration();
		return;
	}

	throw CompileError("unexpected \"" + std::string(current().text) + "\" at top level; "
		"expected a struct, kernel, vertex, fragment or helper function declaration");
}

// A struct or enum tag may share its name with a function or a variable.
bool tagBesideObject(std::string_view a, std::string_view b) {
	const auto isTag = [](std::string_view kindName) { return kindName == "struct" || kindName == "enum"; };
	const auto isObject = [](std::string_view kindName) { return kindName == "function" || kindName == "variable"; };
	return (isTag(a) && isObject(b)) || (isObject(a) && isTag(b));
}

// A struct or enum tag beside any other kind at all (a typedef, an enum constant, a
// function or a variable): wider than tagBesideObject. Apple exempts this wherever a
// using declaration brings the tag in, whether beside another using declaration of one
// alias or beside a name the enclosing scope already declared; it does not exempt a tag
// a using declaration already brought in from a later plain declaration of another kind
// (h2/h3), so declareName's own check, and rejectUsingConflict's, keep tagBesideObject.
bool tagBesideNonTag(std::string_view a, std::string_view b) {
	const auto isTag = [](std::string_view kindName) { return kindName == "struct" || kindName == "enum"; };
	return isTag(a) != isTag(b);
}

// "namespace" is consumed. A name declared in the body is stored under its
// qualified name; an anonymous namespace is the namespace "(anonymous namespace)",
// which its enclosing namespace uses, so its names are visible there unqualified.
void Parser::parseNamespace(bool isInline) {
	const NestingScope nesting(*this);

	const size_t outer = _namespacePath.size();
	if (at(TokenKind::LBrace)) {
		_namespacePath.push_back(kAnonymousNamespace);
		NamespaceScope& parent = _scopes[namespacePrefix(outer)];
		const std::string path = namespacePrefix(outer + 1);
		if (std::find(parent.directives.begin(), parent.directives.end(), path) == parent.directives.end()) {
			parent.directives.push_back(path);
		}
		if (isInline && std::find(parent.inlines.begin(), parent.inlines.end(), path) == parent.inlines.end()) {
			if (_namespaces.count(path)) {
				throw CompileError("the anonymous namespace was declared without inline, and a later definition "
					"cannot make it inline");
			}
			parent.inlines.push_back(path);
		}
		if (isInline) {
			_unit.inlineNamespaces.insert(path);
		}
		_namespaces.insert(path);
	} else {
		while (true) {
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a namespace name, found " + std::string(tokenKindName(kind())));
			}
			const auto enclosing = _scopes.find(namespacePrefix(_namespacePath.size()));
			if (_namespaceAliases.count(qualify(std::string(current().text)))) {
				throw CompileError("\"" + qualify(std::string(current().text)) + "\" is a namespace alias and cannot also be a namespace");
			}
			if (_declared.count(qualify(std::string(current().text)))
				|| (enclosing != _scopes.end() && enclosing->second.declarations.count(std::string(current().text)))) {
				throw CompileError("\"" + qualify(std::string(current().text)) + "\" is declared and "
					"cannot also be a namespace");
			}
			if (_namespacePath.size() > outer && lookahead().kind == TokenKind::Assign) {
				throw CompileError("a namespace alias is declared with one unqualified name, not \""
					+ namespacePrefix(_namespacePath.size()) + "::" + std::string(current().text) + "\"");
			}
			const std::string path = qualify(std::string(current().text));
			if (isInline) {
				if (lookahead().kind == TokenKind::ColonColon) {
					throw CompileError("a nested namespace definition cannot be inline");
				}
				// The first definition decides: a later one may omit inline, none may add it.
				NamespaceScope& parent = _scopes[namespacePrefix(_namespacePath.size())];
				const auto marked = std::find(parent.inlines.begin(), parent.inlines.end(), path);
				if (marked == parent.inlines.end()) {
					if (_namespaces.count(path)) {
						throw CompileError("\"" + path + "\" was declared as a namespace without inline, and a later "
							"definition cannot make it inline");
					}
					parent.inlines.push_back(path);
				}
				_unit.inlineNamespaces.insert(path);
			}
			_namespacePath.emplace_back(advance().text);
			_namespaces.insert(namespacePrefix(_namespacePath.size()));
			if (!match(TokenKind::ColonColon)) {
				break;
			}
		}
		if (at(TokenKind::LBracket)) {
			throw CompileError("an attribute on a namespace is not supported");
		}
	}

	expect(TokenKind::LBrace, "to open a namespace body");
	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		parseTopLevel();
	}
	expect(TokenKind::RBrace, "to close a namespace body");
	_namespacePath.resize(outer);
}

void Parser::parseNamespaceAlias() {
	const std::string name(advance().text);
	expect(TokenKind::Assign, "after the name of a namespace alias");
	QualifiedName spelledTarget;
	if (!peekQualifiedName(spelledTarget)) {
		throw CompileError("expected the namespace that \"" + name + "\" is an alias of");
	}
	for (size_t i = 0; i < spelledTarget.tokens; ++i) {
		advance();
	}
	expect(TokenKind::Semicolon, "after a namespace alias");
	const std::string spelled = (spelledTarget.global ? "::" : "") + joinParts(spelledTarget.parts, 0, spelledTarget.parts.size());
	std::string target = namespaceOf(spelledTarget, spelledTarget.parts.size(), NamespaceLookup::NamespaceOnly);
	// A lone "metal" no file namespace answers for is the library's, and an alias of it
	// carries that identity (kStdlibNamespace) through chains and nested aliases.
	if (target.empty() && spelledTarget.parts.size() == 1 && spelledTarget.parts.front() == kStdlibNamespace) {
		target = kStdlibNamespace;
	}
	if (target.empty()) {
		throw CompileError("\"" + spelled + "\" is not a namespace the file declares");
	}
	if (!_blocks.empty()) {
		if (_blocks.back().names.count(name)
			|| (_blocks.back().sharesEnclosingScope && _blocks.size() >= 2 && _blocks[_blocks.size() - 2].names.count(name))) {
			throw CompileError("\"" + name + "\" is declared and cannot also be a namespace alias");
		}
		if (const auto [entry, inserted] = _blocks.back().aliases.emplace(name, target); !inserted && entry->second != target) {
			throw CompileError("redefinition of \"" + name + "\" as an alias of another namespace (\"" + target
				+ "\", was \"" + entry->second + "\")");
		}
		return;
	}
	const std::string qualified = qualify(name);
	if (qualified == kStdlibNamespace) {
		throw CompileError("redefinition of \"metal\": it is the standard library's namespace");
	}
	const auto enclosing = _scopes.find(namespacePrefix(_namespacePath.size()));
	if (_declared.count(qualified) || (enclosing != _scopes.end() && enclosing->second.declarations.count(name))) {
		throw CompileError("\"" + qualified + "\" is declared and cannot also be a namespace alias");
	}
	if (_namespaces.count(qualified)) {
		throw CompileError("redefinition of \"" + qualified + "\": a namespace and a namespace alias");
	}
	if (const auto [entry, inserted] = _namespaceAliases.emplace(qualified, target); !inserted && entry->second != target) {
		throw CompileError("redefinition of \"" + name + "\" as an alias of another namespace (\"" + target
			+ "\", was \"" + entry->second + "\")");
	}
}

// "using" is consumed: "using namespace N;" or "using N::x;".
void Parser::parseUsing() {
	const std::string scope = namespacePrefix(_namespacePath.size());

	if (matchIdentifier("namespace")) {
		QualifiedName name;
		if (!peekQualifiedName(name)) {
			throw CompileError("expected a namespace name after \"using namespace\"");
		}
		for (size_t i = 0; i < name.tokens; ++i) {
			advance();
		}
		const std::string spelled = (name.global ? "::" : "") + joinParts(name.parts, 0, name.parts.size());
		expect(TokenKind::Semicolon, "after a using directive");

		std::vector<std::string>& directives = _blocks.empty() ? _scopes[scope].directives : _blocks.back().directives;
		// A directive for a namespace its scope already nominates changes no lookup ([namespace.udir]), so it is
		// not recorded again: what counts the usings after a class template sees nothing new.
		const auto record = [&directives](const std::string& nominated) {
			if (std::find(directives.begin(), directives.end(), nominated) == directives.end()) {
				directives.push_back(nominated);
			}
		};

		const std::string target = namespaceOf(name, name.parts.size(), NamespaceLookup::NamespaceOnly);
		// A lone "metal" no file namespace answers for is the standard library's own namespace,
		// which nothing is looked up through; a nearer namespace of that name wins over it.
		if (target.empty() && name.parts.size() == 1 && name.parts.front() == kStdlibNamespace) {
			record(kStdlibNamespace);
			return;
		}
		if (target.empty()) {
			throw CompileError("\"" + spelled + "\" is not a namespace the file declares");
		}
		record(target);
		return;
	}

	// "using T = type;" is "typedef type T;".
	if (at(TokenKind::Identifier) && (lookahead().kind == TokenKind::Assign || lookahead().kind == TokenKind::LBracket)) {
		if (lookahead().kind == TokenKind::LBracket) {
			throw CompileError("an attribute on a type alias is not supported");
		}
		const std::string spelled(current().text);
		const std::string alias = qualify(std::string(advance().text));
		advance();
		if (atKeyword("struct") || atKeyword("enum")) {
			throw CompileError("a \"" + std::string(current().text) + "\" keyword in a type alias is not supported");
		}
		const Type target = parseType();
		if (target.isPointer && target.arrayLength) {
			throw CompileError("a type alias of an array of pointers is not supported");
		}
		if (target.addressSpace != AddressSpace::None && !target.isPointer) {
			throw CompileError("a type alias of a type with an address space is not supported");
		}
		if (!target.namedType.empty() && !_unit.findStruct(target.namedType)) {
			throw CompileError("unknown type name \"" + target.namedType + "\" in a type alias");
		}
		expect(TokenKind::Semicolon, "after a type alias");
		if (!_blocks.empty()) {
			// Only a name nothing else has: one that hid or redefined another would need the
			// scope rules the file-scope table does not have. The block's end removes it.
			Type existing;
			if (blockName(spelled) || _blocks.back().aliases.count(spelled) || _declared.count(alias)
				|| _unit.findStruct(alias) || resolveTypeName(alias, existing)) {
				throw CompileError("the alias \"" + spelled + "\" in a function names something already "
					"declared, which mslc does not support");
			}
			// Bound as a template parameter is: a key no spelling reaches, and the block's name for it,
			// so that a namespace's own or an outer T cannot be found instead.
			const std::string key = "<alias " + spelled + ">";
			_typedefs[key] = target;
			_blocks.back().names[spelled] = key;
			_blocks.back().typedefs.push_back(key);
			return;
		}
		registerTypedef(alias, target);
		return;
	}

	QualifiedName name;
	if (!peekQualifiedName(name) || name.parts.size() < 2) {
		throw CompileError("a using declaration has to name a member of a namespace, as \"using N::x;\"");
	}
	for (size_t i = 0; i < name.tokens; ++i) {
		advance();
	}
	expect(TokenKind::Semicolon, "after a using declaration");
	const std::string resolved = resolveName(name);
	// [namespace.udecl]: the library's namespace has to have the name, and each namespace on
	// the way to it, as for any other one. A namespace the file added to metal is its own.
	const std::string libraryPrefix = std::string(kStdlibNamespace) + "::";
	if (resolved.rfind(libraryPrefix, 0) == 0 && !_declared.count(resolved)) {
		std::string scope = kStdlibNamespace;
		size_t at = libraryPrefix.size();
		while (true) {
			const size_t end = resolved.find("::", at);
			const bool last = end == std::string::npos;
			const std::string member = resolved.substr(at, last ? end : end - at);
			const std::string inner = scope + "::" + member;
			const bool libraryHas = stdlibLookup(scope, member).hasName;
			const bool namespaceOfFile = _namespaces.count(inner) > 0;
			if (last ? !libraryHas : !namespaceOfFile && !(libraryHas && stdlibLookup(inner, "").scopeExists)) {
				throw CompileError(std::string(libraryHas || namespaceOfFile ? "\"" + inner + "\" is not a namespace"
					: "no member named \"" + member + "\" in the namespace \"" + scope + "\"")
					+ " (in the using declaration of \"" + resolved + "\")");
			}
			if (last) {
				break;
			}
			scope = inner;
			at = end + 2;
		}
	}
	if (_blocks.empty() && !_declared.count(resolved) && resolved != "metal::discard_fragment") {
		_scopes[scope].stdlibDeclarations[name.parts.back()].insert(resolved);
		return; // a name of the standard library, which is visible already
	}
	if (_blocks.empty()) {
		// [namespace.udecl]: the name must not already be another declaration of this namespace.
		const std::string& last = name.parts.back();
		const std::string own = scope.empty() ? last : scope + "::" + last;
		std::set<std::string>& brought = _scopes[scope].declarations[last];
		// Functions brought in under one name overload, and a tag may sit beside a function
		// or variable; any other pair of kinds is a clash. metal::discard_fragment, which has
		// no kinds, is a function.
		const auto kindsOrFunction = [&](const std::string& target) {
			std::vector<std::string_view> kinds = kindsOf(target);
			if (kinds.empty()) {
				kinds.push_back("function");
			}
			return kinds;
		};
		const auto clash = [&](const std::string& a, const std::string& b) {
			for (const std::string_view x : kindsOrFunction(a)) {
				for (const std::string_view y : kindsOrFunction(b)) {
					if (!(x == "function" && y == "function") && !tagBesideNonTag(x, y)) {
						return true;
					}
				}
			}
			return false;
		};
		for (const std::string& earlier : brought) {
			if (earlier != resolved && clash(earlier, resolved)) {
				throw CompileError("the using declaration of \"" + resolved + "\" conflicts with the using "
					"declaration of \"" + earlier + "\" in the same namespace");
			}
		}
		// An already-declared name's tag is exempt beside any new using-declared kind, not
		// only a function or variable (g6/h1/h4): a using declaration is the direction
		// Apple accepts this in; the other order (h2/h3) stays narrow, in rejectUsingConflict.
		if (const std::vector<std::string_view> kinds = kindsOf(own);
			!kinds.empty() && conflictsWithUsing(own, kinds, resolved, nullptr, true)) {
			throw CompileError("the using declaration of \"" + resolved + "\" conflicts with \"" + own
				+ "\" declared earlier in the same namespace");
		}
		brought.insert(resolved);
		return;
	}
	if (_blocks.back().sharesEnclosingScope && _blocks.size() >= 2) {
		const auto& outer = _blocks[_blocks.size() - 2].names;
		if (const auto found = outer.find(name.parts.back()); found != outer.end() && found->second != resolved) {
			throw CompileError("the using declaration of \"" + resolved + "\" conflicts with \"" + name.parts.back()
				+ "\" declared earlier in the same scope");
		}
	}
	const auto [entry, inserted] = _blocks.back().names.emplace(name.parts.back(), resolved);
	if (!inserted && entry->second != resolved) {
		throw CompileError("the using declaration of \"" + resolved + "\" conflicts with \"" + name.parts.back()
			+ "\" declared earlier in the same scope");
	}
}

// "thread T&" after a function's return type: the function returns a reference, which is read as a T. Apple requires
// the address space; mslc lowers a thread one only, since a pointer is not a value a SPIR-V function returns here.
void takeReferenceReturn(FunctionDecl& decl, const std::string& quoted) {
	if (decl.returnType.addressSpace == AddressSpace::None) {
		throw CompileError(quoted + " returns a reference without an address space, which MSL requires");
	}
	if (decl.returnType.addressSpace != AddressSpace::Thread || decl.returnType.isPointer) {
		throw CompileError(quoted + " returns a reference to " + std::string(addressSpaceName(decl.returnType.addressSpace))
			+ " memory or to a pointer, which is not lowered yet; mslc lowers a thread reference");
	}
	decl.returnType.addressSpace = AddressSpace::None;
	decl.returnsReference = true;
}

// A thread pointer to a texture or sampler is a return type: what the function returns is resolved while compiling
// (see the emitter's resourceOf), as a texture local is.
// MSL requires the address space (Apple: "pointer type must have explicit address space qualifier").
bool isResourcePointer(const Type& type) {
	return type.isPointer && type.resource != ResourceKind::None && !type.arrayLength
		&& type.addressSpace == AddressSpace::Thread;
}

void rejectUnloweredReturn(const Type& returned, const std::string& quoted, bool allowStdArray) {
	if (isResourcePointer(returned)) {
		return;
	}
	if (returned.isPointer || (returned.arrayLength && !(allowStdArray && returned.isStdArray))
		|| returned.resource != ResourceKind::None
		|| returned.addressSpace != AddressSpace::None) {
		throw CompileError(quoted + " returns a pointer, an array, a texture or sampler, or a value "
			"in an address space, which is not lowered yet; a helper returns a scalar, vector, "
			"matrix or struct value");
	}
}

// The parameters of a helper function, after its opening parenthesis and through
// the closing one.
void Parser::parseHelperParameters(FunctionDecl& decl, const std::string& quoted) {
	// "(void)" is an empty parameter list.
	if (atKeyword("void") && lookahead().kind == TokenKind::RParen) {
		advance();
	}

	std::set<std::string> names;
	while (!at(TokenKind::RParen) && !at(TokenKind::EndOfFile)) {
		decl.parameters.push_back(parseParameter(decl.name));
		const Parameter& parameter = decl.parameters.back();
		if (!parameter.name.empty() && !names.insert(parameter.name).second) {
			throw CompileError("redefinition of parameter \"" + parameter.name + "\" of " + quoted);
		}
		if (at(TokenKind::Assign)) {
			throw CompileError("a default argument is not lowered yet (parameter \"" + parameter.name
				+ "\" of " + quoted + ")");
		}
		if (!match(TokenKind::Comma)) {
			break;
		}
	}

	expect(TokenKind::RParen, "to close a parameter list");
}

// Skips one bracketed group: the opener is the current token, and the token
// after its closer is the current one on return.
void Parser::skipBalanced(TokenKind open, TokenKind close) {
	int depth = 0;
	while (!at(TokenKind::EndOfFile)) {
		if (at(open)) {
			++depth;
		} else if (at(close) && --depth == 0) {
			advance();
			return;
		}
		advance();
	}
	throw CompileError("unbalanced brackets in a template declaration");
}

std::shared_ptr<const Parser::Names> Parser::captureNames() const {
	auto names = std::make_shared<Names>();
	names->typedefs = _typedefs;
	names->enumTypes = _enumTypes;
	names->enumConstants = _enumConstants;
	names->enumUnderlying = _enumUnderlying;
	names->declared = _declared;
	names->declaredBesideTag = _declaredBesideTag;
	names->namespacePath = _namespacePath;
	names->namespaces = _namespaces;
	names->namespaceAliases = _namespaceAliases;
	names->scopes = _scopes;
	names->structs = _unit.structs;
	names->functionTemplates = _unit.functionTemplates;
	names->entryTemplateNames = _entryTemplateNames;
	names->helperNames = _helperNames;
	names->constructors = _constructors;
	names->classTemplates = _classTemplates;
	names->constantInts = _constantInts;
	names->constantIntArrays = _constantIntArrays;
	names->memberTemplateNames = _memberTemplateNames;
	names->seedKeys = _seedKeys;
	return names;
}

void Parser::restoreNames(const Names& names) {
	_typedefs = names.typedefs;
	_enumTypes = names.enumTypes;
	_enumConstants = names.enumConstants;
	_enumUnderlying = names.enumUnderlying;
	_declared = names.declared;
	_declaredBesideTag = names.declaredBesideTag;
	_namespacePath = names.namespacePath;
	_namespaces = names.namespaces;
	_namespaceAliases = names.namespaceAliases;
	_scopes = names.scopes;
	_unit.structs = names.structs;
	_unit.functionTemplates = names.functionTemplates;
	_entryTemplateNames = names.entryTemplateNames;
	_helperNames = names.helperNames;
	_constructors = names.constructors;
	_classTemplates = names.classTemplates;
	_constantInts = names.constantInts;
	_constantIntArrays = names.constantIntArrays;
	_memberTemplateNames = names.memberTemplateNames;
	_seedKeys = names.seedKeys;
	_frozenClasses = true;
}

} // namespace mslc
