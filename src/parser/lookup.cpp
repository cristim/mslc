#include "parser/internal.h"

namespace mslc {

namespace {
	// The nearest namespace that contains both "A::B" and "A::C" ("A"), or "" for the file.
	std::string nearestCommon(const std::string& a, const std::string& b) {
		size_t common = 0;
		size_t at = 0;
		while (true) {
			const size_t endA = a.find("::", at);
			const size_t endB = b.find("::", at);
			const std::string partA = a.substr(at, endA == std::string::npos ? endA : endA - at);
			const std::string partB = b.substr(at, endB == std::string::npos ? endB : endB - at);
			if (partA.empty() || partA != partB) {
				break;
			}
			common = at + partA.size();
			if (endA == std::string::npos || endB == std::string::npos) {
				break;
			}
			at = endA + 2;
		}
		return a.substr(0, common);
	}
}

std::string Parser::namespacePrefix(size_t depth) const {
	return joinParts(_namespacePath, 0, depth);
}

bool Parser::isNamespacePath(const std::string& path) const {
	return path == kStdlibNamespace || _namespaces.count(path) > 0;
}

size_t Parser::libraryPrefixParts(const QualifiedName& name) const {
	for (size_t count = 1; count < name.parts.size(); ++count) {
		const bool literal = name.parts[0] == kStdlibNamespace && count == 1;
		if (literal && !name.global) {
			continue;
		}
		if (literal) {
			return count;
		}
		QualifiedName prefix = name;
		prefix.parts.resize(count);
		if (namespaceOf(prefix, count, NamespaceLookup::NamespaceOnly) == kStdlibNamespace) {
			return count;
		}
	}
	return 0;
}

bool Parser::libraryNominatedFromHere() const {
	const auto reaches = [&](const std::vector<std::string>& targets) {
		for (const std::string& target : targets) {
			std::set<std::string> seen;
			std::set<std::string> none;
			bool nominated = false;
			collectMembers(target, "", seen, none, false, nullptr, nullptr, &nominated);
			if (nominated) {
				return true;
			}
		}
		return false;
	};
	for (size_t depth = 0; depth <= _namespacePath.size(); ++depth) {
		if (const auto entry = _scopes.find(namespacePrefix(depth)); entry != _scopes.end()
			&& reaches(entry->second.directives)) {
			return true;
		}
	}
	for (const BlockScope& block : _blocks) {
		if (reaches(block.directives)) {
			return true;
		}
	}
	return false;
}

size_t Parser::libraryPrefixTokens() const {
	QualifiedName name;
	if (!peekQualifiedName(name)) {
		return 0;
	}
	const size_t parts = libraryPrefixParts(name);
	return parts ? (name.global ? 1 : 0) + 2 * parts : 0;
}

size_t Parser::libraryResourcePrefixTokens() const {
	const size_t tokens = libraryPrefixTokens();
	return tokens && lookahead(tokens).kind == TokenKind::Identifier && isResourceTypeName(lookahead(tokens).text)
		&& lookahead(tokens + 1).kind != TokenKind::ColonColon ? tokens : 0;
}

std::string Parser::canonicalNamespace(const std::string& path) const {
	if (const auto alias = _namespaceAliases.find(path); alias != _namespaceAliases.end()) {
		return alias->second;
	}
	return _namespaces.count(path) ? path : std::string();
}

std::set<std::string> Parser::namespacesNamed(const std::string& scope, const std::string& part, bool qualified,
	std::set<std::string>& seen, NamespaceLookup mode, bool* aliases) const {
	// [namespace.qual], the same tier order collectMembers (above) walks for other names: the
	// namespaces of "scope"; only when it has none, for a qualified name, what each of its
	// using directives (an anonymous namespace's included) finds, each on its own.
	if (!seen.insert(scope).second) {
		return {};
	}
	const std::vector<std::string> set = inlineClosure(scope);   // the own tier: the scope and its inline namespaces
	std::set<std::string> own;
	for (const std::string& at : set) {
		seen.insert(at);
		const std::string key = at.empty() ? part : at + "::" + part;
		// The library's namespace is a member of the file scope, so a directive's metal competes with it.
		if (at.empty() && part == kStdlibNamespace) {
			own.insert(kStdlibNamespace);
		}
		if (std::string found = canonicalNamespace(key); !found.empty()) {
			own.insert(found);
			if (aliases && _namespaceAliases.count(key)) {
				*aliases = true;
			}
		}
		if (mode == NamespaceLookup::NamespaceOnly) {
			continue;
		}
		// [basic.lookup.qual]/1: a struct, enum or typedef "part" names here (directly or
		// through a using declaration) is this scope's own answer too, so it joins the union
		// like any other result (childNamespace rejects it as a namespace itself).
		std::set<std::string> declared;
		if (_declared.count(key)) {
			declared.insert(key);
		}
		if (const auto entry = _scopes.find(at); entry != _scopes.end()) {
			if (const auto brought = entry->second.declarations.find(part); brought != entry->second.declarations.end()) {
				declared.insert(brought->second.begin(), brought->second.end());
			}
		}
		for (const std::string& candidate : declared) {
			for (const std::string_view kind : kindsOf(candidate)) {
				if (kind == "struct" || kind == "enum" || kind == "typedef" || kind == kAnonymousStructTypedef) {
					own.insert(candidate);
				}
			}
		}
	}
	if (!own.empty() || !qualified) {
		return own;
	}
	std::set<std::string> found;
	for (const std::string& at : set) {
		if (const auto entry = _scopes.find(at); entry != _scopes.end()) {
			for (const std::string& directive : entry->second.directives) {
				const std::set<std::string> through = namespacesNamed(directive, part, true, seen, mode, aliases);
				found.insert(through.begin(), through.end());
			}
		}
	}
	return found;
}

std::string Parser::childNamespace(const std::string& scope, const std::string& part, bool qualified, NamespaceLookup mode) const {
	std::set<std::string> seen;
	const std::set<std::string> found = namespacesNamed(scope, part, qualified, seen, mode);
	if (found.size() > 1) {
		throw CompileError("reference to the namespace \"" + part + "\" "
			+ (scope.empty() ? std::string("at file scope") : "in \"" + scope + "\"") + " is ambiguous: it names "
			+ *found.begin() + " and " + *std::next(found.begin()));
	}
	// A struct, enum or typedef namespacesNamed found (not a namespace) is not an answer
	// here, only a competitor for ambiguity above: whatever named it is resolved through
	// resolveName's own fallback instead.
	if (found.empty() || !isNamespacePath(*found.begin())) {
		return std::string();
	}
	return *found.begin();
}

std::string Parser::blockQualifier(const std::string& part, NamespaceLookup mode) const {
	for (auto aliasBlock = _blocks.rbegin(); aliasBlock != _blocks.rend(); ++aliasBlock) {
		const auto alias = aliasBlock->aliases.find(part);
		if (alias == aliasBlock->aliases.end()) {
			continue;
		}
		if (mode == NamespaceLookup::Qualifier) {
			for (auto block = _blocks.rbegin();; ++block) {
				if (const auto binding = block->names.find(part); binding != block->names.end()) {
					for (const std::string_view kind : kindsOf(binding->second)) {
						if (kind == "struct" || kind == "enum" || kind == "typedef" || kind == kAnonymousStructTypedef) {
							if (block != aliasBlock) {
								return binding->second;
							}
							throw CompileError("reference to \"" + part + "\" is ambiguous: it names "
								+ alias->second + " and " + binding->second);
						}
					}
				}
				if (block == aliasBlock) {
					break;
				}
			}
		}
		return alias->second;
	}
	QualifiedName name;
	name.parts.push_back(part);
	return aliasQualifier(name, mode);
}

std::vector<Parser::NamespaceTier> Parser::namespaceTiers(const QualifiedName& name, NamespaceLookup mode) const {
	std::vector<NamespaceTier> tiers;
	const size_t outermost = name.global ? 0 : _namespacePath.size();
	for (size_t depth = outermost + 1; depth-- > 0;) {
		NamespaceTier tier;
		const std::string level = namespacePrefix(depth);
		std::set<std::string> seen;
		tier.found = namespacesNamed(level, name.parts[0], name.global, seen, mode, &tier.aliases);
		if (!name.global) {
			// Transitive nominations retain the original directive's writer, which
			// determines where their candidates join the unqualified lookup tier.
			const auto nominated = [&](const std::string& from, const std::string& target) {
				std::vector<std::string> targets = {target};
				std::set<std::string> visited;
				for (size_t i = 0; i < targets.size(); ++i) {
					if (!visited.insert(targets[i]).second) {
						continue;
					}
					if (nearestCommon(from, targets[i]) == level) {
						std::set<std::string> ownSeen;
						const auto found = namespacesNamed(targets[i], name.parts[0], false, ownSeen, mode, &tier.aliases);
						tier.found.insert(found.begin(), found.end());
					}
					if (const auto entry = _scopes.find(targets[i]); entry != _scopes.end()) {
						targets.insert(targets.end(), entry->second.directives.begin(), entry->second.directives.end());
					}
				}
			};
			for (size_t user = 0; user <= _namespacePath.size(); ++user) {
				for (const std::string& from : inlineClosure(namespacePrefix(user))) {
					if (const auto entry = _scopes.find(from); entry != _scopes.end()) {
						for (const std::string& target : entry->second.directives) {
							nominated(from, target);
						}
					}
				}
			}
			const std::string function = namespacePrefix(_namespacePath.size());
			for (const BlockScope& block : _blocks) {
				for (const std::string& target : block.directives) {
					nominated(function, target);
				}
			}
		}
		tiers.push_back(std::move(tier));
	}
	return tiers;
}

std::string Parser::aliasQualifier(const QualifiedName& name, NamespaceLookup mode) const {
	bool aliasLookup = false;
	for (const auto& tier : namespaceTiers(name, NamespaceLookup::NamespaceOnly)) {
		if (!tier.found.empty()) {
			aliasLookup = tier.aliases;
			break;
		}
	}
	if (!aliasLookup) {
		return {};
	}
	const std::string& part = name.parts[0];
	if (!name.global && mode == NamespaceLookup::Qualifier) {
		for (auto block = _blocks.rbegin(); block != _blocks.rend(); ++block) {
			if (const auto binding = block->names.find(part); binding != block->names.end()) {
				for (const std::string_view kind : kindsOf(binding->second)) {
					if (kind == "struct" || kind == "enum" || kind == "typedef" || kind == kAnonymousStructTypedef) {
						return binding->second;
					}
				}
			}
		}
	}
	for (const auto& tier : namespaceTiers(name, mode)) {
		if (tier.found.size() > 1) {
			throw CompileError("reference to \"" + part + "\" is ambiguous: it names " + *tier.found.begin()
				+ " and " + *std::next(tier.found.begin()));
		}
		if (!tier.found.empty()) {
			return *tier.found.begin();
		}
	}
	return {};
}

// The namespace the first "count" parts of a name spell, found from the current
// namespace outward (or from the file, after a leading "::"), or "" when there is none.
std::string Parser::namespaceOf(const QualifiedName& name, size_t count, NamespaceLookup mode) const {
	const std::vector<std::string>& parts = name.parts;
	// Later parts use qualified own hiding; alias first parts use lexical unions.
	const auto rest = [&](std::string scope) {
		for (size_t part = 1; part < count; ++part) {
			scope = childNamespace(scope, parts[part], true, mode);
			if (scope.empty()) {
				return std::string();
			}
		}
		return scope;
	};
	if (!name.global) {
		if (const std::string candidate = blockQualifier(parts[0], mode); !candidate.empty()) {
			return isNamespacePath(candidate) ? rest(candidate) : std::string();
		}
		// A block's type, or a using declaration of one, is found before any namespace of that name.
		if (mode == NamespaceLookup::Qualifier) {
			for (auto block = _blocks.rbegin(); block != _blocks.rend(); ++block) {
				if (const auto binding = block->names.find(parts[0]); binding != block->names.end()) {
					for (const std::string_view kind : kindsOf(binding->second)) {
						if (kind == "struct" || kind == "enum" || kind == "typedef" || kind == kAnonymousStructTypedef) {
							return std::string();
						}
					}
				}
			}
		}
	} else if (const std::string candidate = aliasQualifier(name, mode); !candidate.empty()) {
		return isNamespacePath(candidate) ? rest(candidate) : std::string();
	}
	// The innermost level that names the first part decides, and two namespaces at one
	// level are ambiguous, as for every other unqualified lookup.
	for (const NamespaceTier& tier : namespaceTiers(name, mode)) {
		if (tier.found.size() > 1) {
			throw CompileError("reference to \"" + parts[0] + "\" is ambiguous: it names " + *tier.found.begin()
				+ " and " + *std::next(tier.found.begin()));
		}
		// [basic.lookup.qual]/1: a struct, enum or typedef found nearer than an outer namespace
		// hides it; the caller then treats the qualifier as the type (an enum's enumerators).
		if (!tier.found.empty()) {
			return _namespaces.count(*tier.found.begin()) ? rest(*tier.found.begin()) : std::string();
		}
	}
	return "";
}

std::string Parser::resolveName(const QualifiedName& name, std::vector<std::string>* alternatives) const {
	std::string resolved = resolveNameUnchecked(name, alternatives);
	// A name an instance finds that was declared after its template (a typedef, a constant, a struct) is not
	// what clang finds from the definition; reading it would give another value, so it is refused. Instances
	// and their members (spelled with "<") and functions (ordered by their own rule) are exempt.
	if (_definedNames && resolved.find('<') == std::string::npos) {
		if (const auto declared = _declared.find(resolved);
			declared != _declared.end() && declared->second != "function" && !_definedNames->count(resolved)) {
			throw ClassTemplateError("the name \"" + resolved + "\" is declared after the class template that uses it; "
				"declare it above the template");
		}
	}
	return resolved;
}

std::string Parser::resolveNameUnchecked(const QualifiedName& name, std::vector<std::string>* alternatives) const {
	const auto pick = [&](const std::set<std::string>& found, const std::string& spelled) {
		// [over.match]: functions that lookup finds in several scopes (using directives and
		// declarations) are one overload set, which the emitter chooses from by the arguments.
		if (found.size() > 1 && alternatives && std::all_of(found.begin(), found.end(), [&](const std::string& each) {
			const std::vector<std::string_view> kinds = kindsOf(each);
			return kinds.size() == 1 && kinds.front() == "function";
		})) {
			alternatives->assign(found.begin(), found.end());
			return *found.begin();
		}
		if (found.size() > 1) {
			throw CompileError("reference to \"" + spelled + "\" is ambiguous: it names " + *found.begin()
				+ " and " + *std::next(found.begin()));
		}
		return *found.begin();
	};

	// An alias of the library's namespace spells its names as "::metal::" does.
	if (const size_t library = libraryPrefixParts(name);
		library && !(name.global && library == 1 && name.parts[0] == kStdlibNamespace)) {
		QualifiedName canonical;
		canonical.global = true;
		canonical.parts.push_back(kStdlibNamespace);
		canonical.parts.insert(canonical.parts.end(), name.parts.begin() + library, name.parts.end());
		return resolveName(canonical);
	}

	const std::string& last = name.parts.back();
	if (name.parts.size() == 1 && !name.global) {
		// The innermost block that declares the name decides: a local hides every
		// namespace's name, and a using declaration there is the answer.
		if (const std::string* bound = blockName(last)) {
			return bound->empty() ? last : *bound;
		}
		std::set<std::string> stdlibTargets;
		if (const std::set<std::string> found = unqualifiedFound(last, nullptr, &stdlibTargets); !found.empty()) {
			return pick(found, last);
		}
		if (!stdlibTargets.empty()) {
			return pick(stdlibTargets, last);
		}
		if (last == "component" && libraryNominatedFromHere()) {
			return "metal::component";
		}
		return last;
	}

	const std::string spelled = (name.global ? "::" : "") + joinParts(name.parts, 0, name.parts.size());
	const std::vector<std::string>& parts = name.parts;
	const std::string scope = parts.size() > 1 ? namespaceOf(name, parts.size() - 1) : std::string();
	const bool found = !scope.empty();

	if (found) {
		std::set<std::string> seen;
		std::set<std::string> members;
		std::set<std::string> beside;
		std::set<std::string> stdlibTargets;
		bool libraryVisible = false;
		collectMembers(scope, last, seen, members, true, &beside, &stdlibTargets, &libraryVisible);
		if (!members.empty()) {
			return pick(members, spelled);
		}
		if (!stdlibTargets.empty()) {
			if (beside.count(last) && stdlibLookup(kStdlibNamespace, last).hasName) {
				stdlibTargets.insert(std::string(kStdlibNamespace) + "::" + last);
			}
			return pick(stdlibTargets, spelled);
		}
		// Nothing the file declares, but the walk reached the library's names ("using
		// metal::name;" or a directive to metal in the namespace or in the namespaces its
		// directives nominate): the name is the library's.
		if (libraryVisible) {
			return std::string(kStdlibNamespace) + "::" + last;
		}
	}

	// "metal::sin" is the standard library's "sin", with or without a using
	// directive, unless the file extends the namespace with a name of that spelling.
	const std::string aliasOwner = !name.global && parts.front() == kStdlibNamespace
		? blockQualifier(parts.front(), NamespaceLookup::Qualifier) : std::string();
	if (parts.size() == 1) {
		// "::x" names a file-scope declaration, or a builtin. A using declaration at file
		// scope adds a name there too, and beside a declaration of its own it is ambiguous.
		if (std::set<std::string> own; collectOwn("", last, own), !own.empty()) {
			return pick(own, spelled);
		}
		std::set<std::string> stdlibTargets;
		if (collectStdlibOwn("", last, &stdlibTargets, nullptr)) {
			return pick(stdlibTargets, spelled);
		}
		return last;
	}
	// "N::access::read" with a directive or alias of metal in N, "precise::sqrt" under
	// "using namespace metal;": a library scope reached through a namespace that nominates
	// metal. The longest prefix that is a file namespace (or the lexical scope) decides.
	if (!found) {
		for (size_t count = parts.size() - 1; count-- > 0;) {
			if (count == 0 && (name.global || parts[0] == kStdlibNamespace)) {
				break;
			}
			if (!stdlibLookup(std::string(kStdlibNamespace) + "::" + parts[count], "").scopeExists) {
				continue;
			}
			bool visible = false;
			if (count == 0) {
				QualifiedName first;
				first.parts.push_back(parts[0]);
				// Before "::" a name finds only namespaces and types: a local, function or variable of
				// that spelling does not hide the library's scope.
				const auto isType = [this](const std::string& candidate) {
					for (const std::string_view kind : kindsOf(candidate)) {
						if (kind == "struct" || kind == "enum" || kind == "typedef" || kind == kAnonymousStructTypedef) {
							return true;
						}
					}
					return false;
				};
				bool typeHides = false;
				if (const std::string* bound = blockName(parts[0])) {
					typeHides = !bound->empty() && isType(*bound);
				} else {
					for (const std::string& candidate : unqualifiedFound(parts[0])) {
						typeHides = typeHides || isType(candidate);
					}
				}
				visible = namespaceOf(first, 1, NamespaceLookup::Qualifier).empty() && !typeHides
					&& libraryNominatedFromHere();
			} else {
				QualifiedName prefix = name;
				prefix.parts.resize(count);
				const std::string from = namespaceOf(prefix, count);
				if (from.empty() || from == kStdlibNamespace) {
					continue;
				}
				std::set<std::string> seen;
				std::set<std::string> members;
				bool nominated = false;
				collectMembers(from, parts[count], seen, members, true, nullptr, nullptr, &nominated);
				// A qualified walk stops at an own member of that name, so "nominated" is set only
				// when the scope declares none.
				visible = nominated;
			}
			if (visible) {
				QualifiedName canonical;
				canonical.global = true;
				canonical.parts.push_back(kStdlibNamespace);
				canonical.parts.insert(canonical.parts.end(), parts.begin() + count, parts.end());
				return resolveName(canonical);
			}
		}
	}
	if (!found && parts.size() > 1) {
		QualifiedName owner = name;
		owner.parts.pop_back();
		std::string ownerName;
		if (owner.parts.size() > 1) {
			const std::string enclosing = namespaceOf(owner, owner.parts.size() - 1);
			if (!enclosing.empty()) {
				std::set<std::string> seen;
				const std::set<std::string> qualifiers = namespacesNamed(enclosing, owner.parts.back(), true, seen, NamespaceLookup::Qualifier);
				if (!qualifiers.empty()) {
					ownerName = pick(qualifiers, spelled);
				}
			}
		}
		if (ownerName.empty()) {
			if (owner.parts.size() == 1) {
				const std::string candidate = owner.global ? aliasQualifier(owner, NamespaceLookup::Qualifier)
					: blockQualifier(owner.parts[0], NamespaceLookup::Qualifier);
				if (!candidate.empty() && !_namespaces.count(candidate)) {
					ownerName = candidate;
				}
			}
		}
		if (ownerName.empty()) {
			ownerName = resolveName(owner);
		}
		// "S::member": a member function of a struct is named by the struct.
		if (!name.global && _unit.findStruct(ownerName)) {
			return ownerName + "::" + last;
		}
		// "E::v" and "A::E::v": an unscoped enum's name qualifies its enumerators,
		// which are declared in the enum's enclosing namespace.
		Type ownerType;
		if (resolveTypeName(ownerName, ownerType) && !ownerType.enumName.empty()) {
			const std::string& tag = ownerType.enumName;
			const size_t enclosing = tag.rfind("::");
			const std::string key = tag == "metal::component" ? tag + "::" + last
				: enclosing == std::string::npos ? last : tag.substr(0, enclosing + 2) + last;
			const auto constant = _enumConstants.find(key);
			if (constant != _enumConstants.end() && constant->second.owner == tag) {
				return key;
			}
			// "typedef enum { v } T;": v's owner is "<anonymous>", which can not be told
			// from another anonymous enum's, so T::v (and a typedef of T) is not resolved.
			if (constant != _enumConstants.end() && constant->second.owner == "<anonymous>"
				&& _declared.at(tag) == "typedef") {
				throw CompileError("\"" + spelled + "\" qualifies an enumerator by the typedef of an anonymous "
					"enum, which mslc does not support; write \"" + last + "\"");
			}
			throw CompileError("no member named \"" + last + "\" in the enum \"" + ownerName + "\" (in \""
				+ spelled + "\")");
		}
	}
	// A nearer namespace named metal hides the library's: its own members are all there are.
	if (!name.global && parts.size() > 1 && parts.front() == kStdlibNamespace) {
		QualifiedName first;
		first.parts.push_back(parts.front());
		if (const std::string nearer = namespaceOf(first, 1, NamespaceLookup::Qualifier);
			!nearer.empty() && nearer != kStdlibNamespace && isNamespacePath(nearer)) {
			throw CompileError("no member named \"" + (found ? last : parts[1]) + "\" in the namespace \"" + nearer
				+ "\" (in \"" + spelled + "\")");
		}
	}
	if (parts.front() == kStdlibNamespace && parts.size() > 1
		&& (aliasOwner.empty() || _namespaces.count(aliasOwner))
		&& (parts.size() != 2 || last != "discard_fragment" || !found)) {
		return joinParts(parts, 0, parts.size());
	}
	// mem_flags is an enum class of the standard library; the barrier lowering reads its enumerators.
	if (!found && parts.size() == 2 && parts.front() == "mem_flags") {
		return spelled;
	}
	if (!found) {
		throw CompileError("\"" + joinParts(parts, 0, parts.size() - 1) + "\" in \"" + spelled
			+ "\" is not a namespace the file declares");
	}
	throw CompileError("no member named \"" + last + "\" in the namespace \"" + scope + "\" (in \"" + spelled + "\")");
}

std::set<std::string> Parser::unqualifiedFound(const std::string& last, std::set<std::string>* stdlibBeside,
	std::set<std::string>* stdlibTargets) const {
	// [namespace.udir]: a using directive in U nominating T makes T's names
	// appear in the nearest namespace L containing both, for lookups from inside U.
	// So each enclosing namespace L, innermost out, offers its own names plus
	// those of directives that land in it; the first level that has any decides.
	std::set<std::string> nearerBeside;
	for (size_t depth = _namespacePath.size() + 1; depth-- > 0;) {
		const std::string level = joinParts(_namespacePath, 0, depth);
		std::set<std::string> found, levelStdlib, hereBeside;
		collectOwn(level, last, found);
		collectStdlibOwn(level, last, &levelStdlib, &hereBeside);
		if (level == kStdlibNamespace || level == "metal::precise" || level == "metal::fast") {
			hereBeside.insert(level == kStdlibNamespace ? last : level.substr(7) + "::" + last);
		}
		for (size_t user = 0; user <= _namespacePath.size(); ++user) {
			for (const std::string& from : inlineClosure(joinParts(_namespacePath, 0, user))) {
				const auto entry = _scopes.find(from);
				if (entry == _scopes.end()) {
					continue;
				}
				for (const std::string& target : entry->second.directives) {
					if (nearestCommon(from, target) == level) {
						std::set<std::string> seen;
						collectMembers(target, last, seen, found, false, &hereBeside, &levelStdlib);
					}
				}
			}
		}
		const std::string function = namespacePrefix(_namespacePath.size());
		for (const BlockScope& block : _blocks) {
			for (const std::string& target : block.directives) {
				if (nearestCommon(function, target) == level) {
					std::set<std::string> seen;
					collectMembers(target, last, seen, found, false, &hereBeside, &levelStdlib);
				}
			}
		}
		// "using metal::name;" makes the library's name this level's own, which hides every
		// outer declaration of it: the name is the library's.
		if (found.empty() && !levelStdlib.empty()) {
			if (hereBeside.count(last) && stdlibLookup(kStdlibNamespace, last).hasName) {
				levelStdlib.insert(std::string(kStdlibNamespace) + "::" + last);
			}
			if (stdlibTargets) {
				*stdlibTargets = std::move(levelStdlib);
			}
			return {};
		}
		if (!found.empty()) {
			if (stdlibBeside) {
				stdlibBeside->insert(nearerBeside.begin(), nearerBeside.end());
				stdlibBeside->insert(hereBeside.begin(), hereBeside.end());
			}
			return found;
		}
		// Namespace metal's own names may hide the user's function found further out.
		nearerBeside.insert(hereBeside.begin(), hereBeside.end());
	}
	return {};
}

std::string Parser::peekResolved(size_t& tokens, std::vector<std::string>* alternatives) const {
	QualifiedName name;
	if (!peekQualifiedName(name)) {
		throw CompileError("expected a name, found " + std::string(tokenKindName(kind())) + " \""
			+ std::string(current().text) + "\"");
	}
	tokens = name.tokens;
	return resolveName(name, alternatives);
}

} // namespace mslc
