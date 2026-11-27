#include "parser/internal.h"

namespace mslc {

bool sameType(const Type& a, const Type& b) {
	return a.scalar == b.scalar && a.vectorWidth == b.vectorWidth
		&& a.matrixColumns == b.matrixColumns && a.isPacked == b.isPacked
		&& a.namedType == b.namedType && a.isConst == b.isConst
		&& a.enumName == b.enumName && a.isAtomic == b.isAtomic
		&& a.arrayLength == b.arrayLength && a.innerArrayLengths == b.innerArrayLengths
		&& a.isStdArray == b.isStdArray;
}


bool isTypeSupportKind(std::string_view what) {
	return what == "typedef" || what == "enum" || what == "enum constant"
		|| what == kAnonymousStructTypedef;
}


bool Parser::resolveTypeName(std::string_view text, Type& out) const {
	std::string_view builtin = text;
	if (text.substr(0, 7) == "metal::" && !_declared.count(std::string(text))) {
		const auto candidate = text.substr(7);
		if (isMetalBuiltinTypeName(candidate)) {
			builtin = candidate;
		}
	}
	if (isTypeName(builtin, out) || isSimdVectorName(text, out)) {
		if (out.isPacked && out.scalar == ScalarKind::Bool) {
			throw CompileError("\"" + std::string(text) + "\" is not supported: a bool vector has "
				"no storage layout in mslc");
		}

		return true;
	}

	if (text.substr(0, 7) == "packed_") {
		Type rest;
		if (isUnpackedTypeName(text.substr(7), rest) && rest.isVector() && !rest.isMatrix()) {
			throw CompileError("\"" + std::string(text) + "\" is not supported: mslc has packed "
				"vectors of 2 to 4 components of the integer, half and float types");
		}
	}

	// An unscoped enum without a fixed underlying type is stored as an int, or as
	// an unsigned int when a value passes INT_MAX, and promotes to that type in
	// arithmetic, so it is that type here. sema rejects an int or another enum
	// where an enum is expected in an initializer or an assignment
	// (requireEnumValue); other conversion sites are tracked in #235.
	if (const auto enumType = _enumTypes.find(std::string(text)); enumType != _enumTypes.end()
		&& !(enumType->second == "metal::component" && _declared.count("metal::component"))) {
		out.scalar = _enumUnderlying.at(enumType->second);
		out.vectorWidth = 0;
		out.matrixColumns = 0;
		out.isPacked = false;
		out.namedType.clear();
		out.enumName = enumType->second;
		return true;
	}

	if (builtin == "atomic_uint" || builtin == "atomic_int") {
		out.scalar = builtin == "atomic_uint" ? ScalarKind::UInt : ScalarKind::Int;
		out.vectorWidth = 0;
		out.matrixColumns = 0;
		out.isPacked = false;
		out.namedType.clear();
		out.isAtomic = true;
		return true;
	}

	const auto alias = _typedefs.find(std::string(text));
	if (alias == _typedefs.end()) {
		return false;
	}

	out.scalar = alias->second.scalar;
	out.vectorWidth = alias->second.vectorWidth;
	out.matrixColumns = alias->second.matrixColumns;
	out.isPacked = alias->second.isPacked;
	out.namedType = alias->second.namedType;
	out.enumName = alias->second.enumName;
	out.isAtomic = alias->second.isAtomic;
	if (alias->second.isPointer) {
		out.isPointer = true;
		out.addressSpace = alias->second.addressSpace;
	}
	out.arrayLength = alias->second.arrayLength;
	out.innerArrayLengths = alias->second.innerArrayLengths;
	out.isStdArray = alias->second.isStdArray;
	// Only a function template's type parameter deduced from a texture or a sampler argument is one here: a
	// typedef of a resource type is refused where it is written.
	out.resource = alias->second.resource;
	out.textureAccess = alias->second.textureAccess;
	out.isDepth = alias->second.isDepth;
	out.isMultisampled = alias->second.isMultisampled;
	out.isConst = out.isConst || alias->second.isConst;
	return true;
}

void Parser::declareName(const std::string& name, const char* what) {
	if ((_unit.functionTemplates.count(name) || _entryTemplateNames.count(name))
		&& std::string_view(what) != "function") {
		throw CompileError("\"" + name + "\" is declared as " + what + " beside a function template of the "
			"same name, which is not lowered: a call spelled like a construction would not reach the template");
	}
	// The simd names are global typedefs in Apple's compiler, so nothing at file
	// scope can reuse one. The builtin MSL names are not: a struct may be called
	// float2, and a typedef or an enumerator may not.
	Type builtin;
	const size_t tail = name.rfind("::");
	const std::string unqualified = tail == std::string::npos ? name : name.substr(tail + 2);
	if (isSimdVectorName(unqualified, builtin) || (isTypeSupportKind(what) && isTypeName(unqualified, builtin))) {
		throw CompileError("\"" + name + "\" is a builtin type name and cannot be redeclared as a "
			+ what);
	}

	if (_namespaces.count(name)) {
		throw CompileError("redefinition of \"" + name + "\": declared as namespace and again as " + what);
	}
	if (_namespaceAliases.count(name)) {
		throw CompileError("redefinition of \"" + name + "\": declared as namespace alias and again as " + what);
	}

	// Functions overload, and a struct or enum tag may share a name with a function
	// or a variable; every other pair of declarations of one name is a redefinition.
	const auto [existing, inserted] = _declared.emplace(name, what);
	const std::string_view now(what);
	const bool overload = now == "function" && existing->second == "function";
	const bool besideTag = tagBesideObject(now, existing->second);
	if (!inserted && !overload && !besideTag) {
		throw CompileError("redefinition of \"" + name + "\": declared as " + existing->second
			+ " and again as " + what);
	}
	if (besideTag) {
		// A tag and an object: a second object of the name is checked against the first.
		const auto [second, first] = _declaredBesideTag.emplace(name, what);
		if (!first && !(now == "function" && second->second == "function")) {
			throw CompileError("redefinition of \"" + name + "\": declared as " + second->second
				+ " and again as " + what);
		}
	}

	// A function is checked once its parameters are known; an enumerator declared after a
	// using declaration of its name is accepted, as Apple does, and is ambiguous where used.
	if (std::string_view(what) != "function" && std::string_view(what) != "enum constant") {
		rejectUsingConflict(name, what, nullptr);
	}
}

std::vector<std::string_view> Parser::kindsOf(const std::string& name) const {
	std::vector<std::string_view> kinds;
	if (const auto first = _declared.find(name); first != _declared.end()) {
		kinds.push_back(first->second);
	}
	if (const auto second = _declaredBesideTag.find(name); second != _declaredBesideTag.end()) {
		kinds.push_back(second->second);
	}
	return kinds;
}

// A function or a variable of a name hides a struct or enum tag of that name, so
// "S(...)" and "S{...}" name the object and not the type ([basic.scope.hiding]).
// A field or member function of the struct whose function body is being read hides a type of its
// name, through the implicit this (the whole struct is in scope there, whatever order it is written in).
bool Parser::memberHidesName(const std::string& name) const {
	return _memberFields && isLocal("this")
		&& (_memberFields->count(name) || _memberFunctions.count(name));
}

bool Parser::objectHidesTag(const std::string& name) const {
	for (const std::string_view kindName : kindsOf(name)) {
		if (kindName == "function" || kindName == "variable") {
			return true;
		}
	}
	return false;
}

bool Parser::conflictsWithUsing(const std::string& own, const std::vector<std::string_view>& ownKinds,
	const std::string& target, const FunctionDecl* function, bool wideTagExemption) const {
	const auto named = [&](const std::string& name, const auto& visit) {
		for (const std::vector<FunctionDecl>* list : {&_unit.helpers, &_unit.functions}) {
			for (const FunctionDecl& decl : *list) {
				if (decl.name == name && visit(decl)) {
					return true;
				}
			}
		}
		return false;
	};
	const auto sameAsTarget = [&](const FunctionDecl& candidate) {
		return named(target, [&](const FunctionDecl& decl) { return sameParameterTypes(candidate, decl); });
	};
	// metal::discard_fragment has no kinds, and no file declaration is checked against it.
	for (const std::string_view targetKind : kindsOf(target)) {
		for (const std::string_view ownKind : ownKinds) {
			if (wideTagExemption ? tagBesideNonTag(ownKind, targetKind) : tagBesideObject(ownKind, targetKind)) {
				continue;
			}
			if (ownKind != "function" || targetKind != "function"
				|| (function ? sameAsTarget(*function) : named(own, sameAsTarget))) {
				return true;
			}
		}
	}
	return false;
}

void Parser::rejectUsingConflict(const std::string& name, std::string_view what, const FunctionDecl* function) const {
	const size_t tail = name.rfind("::");
	const auto entry = _scopes.find(tail == std::string::npos ? std::string() : name.substr(0, tail));
	if (entry == _scopes.end()) {
		return;
	}
	const auto brought = entry->second.declarations.find(tail == std::string::npos ? name : name.substr(tail + 2));
	if (brought == entry->second.declarations.end()) {
		return;
	}
	for (const std::string& target : brought->second) {
		// A later plain declaration against a using declaration already in scope keeps the
		// narrow exemption (tag beside a function or variable only): a using-declared tag
		// is not hidden by a typedef or enumerator declared after it (h2, h3).
		if (conflictsWithUsing(name, {what}, target, function, false)) {
			throw CompileError("\"" + name + "\" conflicts with the using declaration of \"" + target
				+ "\" in the same namespace");
		}
	}
}

void Parser::declareLocal(const std::string& name) {
	if (!_blocks.empty() && _blocks.back().aliases.count(name)) {
		throw CompileError("\"" + name + "\" conflicts with a namespace alias in the same scope");
	}
	// A name that resolves to a declaration inside a namespace is hidden the same way. A
	// declaration is not a use, so several declarations the name finds are not ambiguous.
	const std::string* bound = blockName(name);
	const std::set<std::string> keys = bound && !bound->empty() ? std::set<std::string>{*bound} : unqualifiedFound(name);
	for (const std::string& key : keys) {
		const char* what = _enumConstants.count(key) ? "enum constant"
			: _typedefs.count(key) ? "typedef"
			: _enumTypes.count(key) ? "enum type"
			: nullptr;
		if (what) {
			throw CompileError("a variable named \"" + name + "\" would hide the " + std::string(what) + " of "
				"that name, which mslc does not support");
		}
	}

	if (_blocks.empty()) {
		return;
	}
	// The outermost block of a function or a for body is the same C++ scope as the
	// parameters or the init declaration, so it cannot redeclare one. Lookup still walks
	// the two blocks separately.
	if (_blocks.back().sharesEnclosingScope && _blocks.size() >= 2 && _blocks[_blocks.size() - 2].names.count(name)) {
		throw CompileError("redefinition of \"" + name + "\" in the same scope");
	}
	const auto [entry, inserted] = _blocks.back().names.emplace(name, "");
	if (!inserted) {
		throw CompileError(entry->second.empty() ? "redefinition of \"" + name + "\" in the same scope"
			: "\"" + name + "\" conflicts with the using declaration of \"" + entry->second + "\" in the same scope");
	}
}

const std::string* Parser::blockName(const std::string& name) const {
	for (auto scope = _blocks.rbegin(); scope != _blocks.rend(); ++scope) {
		if (const auto entry = scope->names.find(name); entry != scope->names.end()) {
			return &entry->second;
		}
	}
	return nullptr;
}

bool Parser::namesDependentParameter(const std::string& name) const {
	for (size_t i = _blocks.size(); i-- > 0;) {
		if (_blocks[i].names.count(name)) {
			return i == _parameterBlock && _dependentParameters.count(name);
		}
	}
	return false;
}

bool Parser::typeTokensIndependent(size_t from, size_t to) const {
	if (to == SIZE_MAX) {
		to = _position;
	}
	for (size_t i = from; i < to; ++i) {
		if (_tokens[i].kind != TokenKind::Identifier) {
			continue;
		}
		const std::string text(_tokens[i].text);
		if (text == "auto" || text == "decltype" || text == "typeof" || text == "__typeof__") {
			return false;
		}
		// A using-declaration of a block names a namespace member, which is not dependent; a template parameter
		// ("<template T>") and a local are.
		if (const std::string* bound = blockName(text); bound && (bound->empty() || bound->front() == '<')) {
			return false;
		}
	}
	return true;
}

// "auto a = B{...};" with B a plain struct has the type B whatever the arguments are.
void Parser::noteAutoConstruction(const std::string& name, size_t from) {
	if (_localIndependent || _blocks.empty() || _position < from + 3) {
		return;
	}
	const Token& head = _tokens[from];
	const Token& open = _tokens[from + 1];
	if (head.kind != TokenKind::Identifier || (open.kind != TokenKind::LBrace && open.kind != TokenKind::LParen)
		|| !typeTokensIndependent(from, from + 1)) {
		return;
	}
	const std::string text(head.text);
	if (!_unit.findStruct(text) && !_unit.findStruct(qualify(text))) {
		return;
	}
	const TokenKind closer = open.kind == TokenKind::LBrace ? TokenKind::RBrace : TokenKind::RParen;
	int depth = 0;
	for (size_t i = from + 1; i < _position; ++i) {
		const TokenKind k = _tokens[i].kind;
		depth += (k == TokenKind::LBrace || k == TokenKind::LParen || k == TokenKind::LBracket) ? 1
			: (k == TokenKind::RBrace || k == TokenKind::RParen || k == TokenKind::RBracket) ? -1 : 0;
		if (depth == 0) {
			if (i + 1 == _position && _tokens[i].kind == closer) {
				_blocks.back().independent.insert(name);
			}
			return;
		}
	}
}

void Parser::noteLocal(const std::string& name) {
	if (_localIndependent && !_blocks.empty()) {
		_blocks.back().independent.insert(name);
		if (_localInjected) {
			_blocks.back().injected.insert(name);
		}
	}
}

bool Parser::classContext() const {
	return _classMemberInstance || _classDepth > 0;
}

// True when the type names the current class template without arguments (the injected class name).
bool Parser::namesInjectedClass(size_t from, size_t to) const {
	if (to == SIZE_MAX) {
		to = _position;
	}
	if (!classContext()) {
		return false;
	}
	for (size_t i = from; i < to; ++i) {
		if (_tokens[i].kind != TokenKind::Identifier || (i + 1 < _tokens.size() && _tokens[i + 1].kind == TokenKind::Less)) {
			continue;
		}
		const std::string text(_tokens[i].text);
		if (text == unqualifiedName(_memberStruct)) {
			return true;
		}
		for (const std::string& building: _classInProgress) {
			if (text == unqualifiedName(building)) {
				return true;
			}
		}
	}
	return false;
}

bool Parser::injectedLocal(const std::string& name) const {
	for (size_t i = _blocks.size(); i-- > 0;) {
		if (_blocks[i].names.count(name)) {
			return _blocks[i].injected.count(name) != 0;
		}
	}
	return false;
}

// A member reached through an injected-name local, directly or through elements, has a type built from the class's
// own parameters.
bool Parser::injectedBase(const Expression& chain) const {
	const Expression* base = &chain;
	while (base->kind == ExpressionKind::Member || base->kind == ExpressionKind::Index) {
		base = base->left.get();
	}
	return classContext() && base->kind == ExpressionKind::Identifier && injectedLocal(base->name);
}

bool Parser::receiverIndependent(const Expression& receiver) const {
	switch (receiver.kind) {
	case ExpressionKind::Identifier:
		if (receiver.name == "this") {
			return true;
		}
		for (size_t i = _blocks.size(); i-- > 0;) {
			if (_blocks[i].names.count(receiver.name)) {
				return i == _parameterBlock ? !classContext() && !_dependentParameters.count(receiver.name)
					: _blocks[i].independent.count(receiver.name) != 0;
			}
		}
		return false;
	// A field of "this" has the declared type of a plain struct's field; in a class template it may be a parameter.
	// A field of an object, or an element of an array or a pointer, of a type naming no parameter is as well.
	case ExpressionKind::Member:
		return receiver.left->kind == ExpressionKind::Identifier && receiver.left->name == "this" ? !classContext()
			: injectedBase(*receiver.left) ? false
			: receiverIndependent(*receiver.left);
	case ExpressionKind::Index: {
		// A dependent index makes the element dependent, as a dependent call argument does.
		const ExpressionKind k = receiver.arguments.front()->kind;
		return receiverIndependent(*receiver.left) && (k == ExpressionKind::IntLiteral
			|| receiverIndependent(*receiver.arguments.front()));
	}
	// A call has a type that depends on nothing only when the function is a plain one, and its arguments are
	// literals or independent themselves (a dependent argument makes the call dependent through lookup).
	case ExpressionKind::Call: {
		if (classContext() || receiver.left->kind != ExpressionKind::Identifier || blockName(receiver.left->name)
			|| !receiver.left->overloadNames.empty() || _unit.functionTemplates.count(receiver.left->name)) {
			return false;
		}
		const auto declared = _declared.find(receiver.left->name);
		if (declared == _declared.end() || declared->second != "function") {
			return false;
		}
		for (const ExpressionPtr& argument: receiver.arguments) {
			const ExpressionKind k = argument->kind;
			if (k != ExpressionKind::IntLiteral && k != ExpressionKind::FloatLiteral && k != ExpressionKind::BoolLiteral
				&& !receiverIndependent(*argument)) {
				return false;
			}
		}
		return true;
	}
	default:
		return false;
	}
}

bool Parser::isLocal(const std::string& name) const {
	const std::string* bound = blockName(name);
	return bound && bound->empty();
}

std::string Parser::qualify(const std::string& name) const {
	std::string out;
	for (const std::string& part : _namespacePath) {
		out += part + "::";
	}
	return out + name;
}

bool Parser::peekQualifiedName(QualifiedName& out) const {
	size_t at = 0;
	out = QualifiedName();
	if (lookahead(at).kind == TokenKind::ColonColon) {
		out.global = true;
		++at;
	}
	while (lookahead(at).kind == TokenKind::Identifier) {
		out.parts.emplace_back(lookahead(at).text);
		++at;
		// "Name<arguments>": the name of a class template instance, which the first use declares.
		if (!_classTemplates.empty() && lookahead(at).kind == TokenKind::Less) {
			if (const ClassTemplate* declaration = classTemplateNamed(out.parts, out.global)) {
				Parser& self = const_cast<Parser&>(*this);
				const size_t saved = _position;
				self._position = saved + at;
				try {
					std::vector<TemplateArgument> arguments = self.parseClassTemplateArguments(*declaration);
					at = self._position - saved;
					const std::string instance = self.instantiateClass(*declaration, std::move(arguments));
					out.parts.back() = instance.substr(declaration->name.size() - declaration->spelled.size());
				} catch (const ClassTemplateError&) {
					self._position = saved;
					throw;
				} catch (const CompileError& error) {
					self._position = saved;
					throw ClassTemplateError(std::string("in the template arguments of \"") + declaration->name + "\": " + error.what());
				}
				self._position = saved;
			}
		}
		// A qualifier, address space or builtin scalar keyword is never a namespace: in
		// "constexpr ::metal::sampler" or "void ::N::k", "::" begins the next name.
		if (out.parts.size() == 1 && !out.global && (isTypeQualifier(out.parts[0]) || addressSpaceFor(out.parts[0])
			|| isIntegerSpecifier(out.parts[0]) || out.parts[0] == "void" || out.parts[0] == "float"
			|| out.parts[0] == "double" || out.parts[0] == "half" || out.parts[0] == "bool")) {
			break;
		}
		if (lookahead(at).kind != TokenKind::ColonColon || lookahead(at + 1).kind != TokenKind::Identifier) {
			break;
		}
		++at;
	}
	out.tokens = at;
	return !out.parts.empty();
}

void Parser::collectOwn(const std::string& scope, const std::string& name, std::set<std::string>& found) const {
	if (scope == kStdlibNamespace && name == "discard_fragment") {
		found.insert("metal::discard_fragment");
	}
	std::set<std::string> local;
	const std::string key = scope.empty() ? name : scope + "::" + name;
	if (_declared.count(key)) {
		local.insert(key);
	}
	if (const auto entry = _scopes.find(scope); entry != _scopes.end()) {
		if (const auto brought = entry->second.declarations.find(name); brought != entry->second.declarations.end()) {
			local.insert(brought->second.begin(), brought->second.end());
		}
	}
	// A function, variable or enumerator this scope declares, directly or through a
	// using declaration, hides a struct or enum tag of the same name in value lookup
	// (g1/g2/g4/g6, h4, h6-h8): calling or reading the name finds that one, not
	// "ambiguous" with the tag it would otherwise look like beside it. A typedef does
	// not hide a tag this way (g7, j5 stay ambiguous: a typedef names a type, same as
	// the tag, so a use of the plain name cannot tell which type is meant). This
	// combines only what one scope contributes; a directive's own members are
	// collected by a separate call (g3), so two directives bringing in a struct and a
	// function stay ambiguous, as Apple reports it.
	const auto isPureTag = [this](const std::string& target) {
		const std::vector<std::string_view> kinds = kindsOf(target);
		if (kinds.empty()) {
			return false;
		}
		for (const std::string_view kind : kinds) {
			if (kind != "struct" && kind != "enum") {
				return false;
			}
		}
		return true;
	};
	const auto isHider = [this](const std::string& target) {
		for (const std::string_view kind : kindsOf(target)) {
			if (kind == "function" || kind == "variable" || kind == "enum constant") {
				return true;
			}
		}
		return false;
	};
	bool hasHider = false;
	for (const std::string& candidate : local) {
		if (isHider(candidate)) {
			hasHider = true;
			break;
		}
	}
	for (const std::string& candidate : local) {
		if (!hasHider || !isPureTag(candidate)) {
			found.insert(candidate);
		}
	}
	// An inline namespace's members are this scope's own.
	if (const auto entry = _scopes.find(scope); entry != _scopes.end()) {
		for (const std::string& inner : entry->second.inlines) {
			collectOwn(inner, name, found);
		}
	}
}

std::vector<std::string> Parser::inlineClosure(const std::string& scope) const {
	std::vector<std::string> set = {scope};
	for (size_t i = 0; i < set.size(); ++i) {
		if (const auto entry = _scopes.find(set[i]); entry != _scopes.end()) {
			set.insert(set.end(), entry->second.inlines.begin(), entry->second.inlines.end());
		}
	}
	return set;
}

bool Parser::collectStdlibOwn(const std::string& scope, const std::string& name,
	std::set<std::string>* targets, std::set<std::string>* beside) const {
	bool found = false;
	for (const std::string& inner : inlineClosure(scope)) {
		if (const auto entry = _scopes.find(inner); entry != _scopes.end()) {
			if (const auto stdlib = entry->second.stdlibDeclarations.find(name); stdlib != entry->second.stdlibDeclarations.end()) {
				found = true;
				if (targets) { targets->insert(stdlib->second.begin(), stdlib->second.end()); }
				for (const std::string& target : stdlib->second) {
					if (beside && target.compare(0, 7, "metal::") == 0) { beside->insert(target.substr(7)); }
				}
			}
		}
	}
	return found;
}

void Parser::collectMembers(const std::string& scope, const std::string& name, std::set<std::string>& seen,
	std::set<std::string>& found, bool qualified, std::set<std::string>* stdlibBeside,
	std::set<std::string>* stdlibTargets, bool* reachedMetal) const {
	if (!seen.insert(scope).second) {
		return;
	}
	// [namespace.qual]: a qualified name searches a namespace's directives only when
	// the namespace itself declares nothing of that name. namespacesNamed (below) walks a
	// namespace name the same way, through canonicalNamespace instead of collectOwn.
	std::set<std::string> own;
	collectOwn(scope, name, own);
	found.insert(own.begin(), own.end());
	const auto entry = _scopes.find(scope);
	// "using metal::name;" makes the library's name this namespace's own member too.
	const bool declaresStdlib = collectStdlibOwn(scope, name, stdlibTargets, stdlibBeside);
	if (reachedMetal && scope == kStdlibNamespace) {
		*reachedMetal = true;
	}
	if (stdlibBeside && !name.empty()
		&& (scope == kStdlibNamespace || scope == "metal::precise" || scope == "metal::fast")) {
		stdlibBeside->insert(scope == kStdlibNamespace ? name : scope.substr(7) + "::" + name);
	}
	if (qualified && (!own.empty() || declaresStdlib)) {
		return;
	}
	if (entry == _scopes.end()) {
		return;
	}
	// An inline namespace's directives are its enclosing namespace's too.
	for (const std::string& writer : inlineClosure(scope)) {
		if (const auto written = _scopes.find(writer); written != _scopes.end()) {
			for (const std::string& directive : written->second.directives) {
				collectMembers(directive, name, seen, found, qualified, stdlibBeside, stdlibTargets, reachedMetal);
			}
		}
	}
}

} // namespace mslc
