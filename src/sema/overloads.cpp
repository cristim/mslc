#include "sema/emitter.h"

namespace mslc {

// The names a call can denote: the one the parser resolved, the others it found at the same
// level (using directives and declarations), and for an unqualified call the same-named
// function of the namespace of each struct argument (argument-dependent lookup).
// The struct an argument has the type of, for argument-dependent lookup: structOf, and the
// forms that only keep their operand's type (a comma, a conditional).
// Kept apart from structOf, which member access calls on any expression.
const StructDecl* Emitter::adlStructOf(const Expression& argument) {
	switch (argument.kind) {
		case ExpressionKind::Comma: return adlStructOf(*argument.right);
		case ExpressionKind::Conditional: {
			const StructDecl* first = adlStructOf(*argument.arguments.at(0));
			return first == adlStructOf(*argument.arguments.at(1)) ? first : nullptr;
		}
		default: return structOf(argument);
	}
}

std::vector<std::string> Emitter::namesOfCall(const Expression& call, std::set<std::string>* adlOnly) {
	std::vector<std::string> names = call.left->overloadNames;
	if (names.empty()) {
		names.push_back(call.left->name);
	}
	if (!call.left->unqualifiedCall) {
		return names;
	}
	const size_t tail = call.left->name.rfind("::");
	const std::string last = tail == std::string::npos ? call.left->name : call.left->name.substr(tail + 2);
	for (const ExpressionPtr& argument: call.arguments) {
		const StructDecl* decl = adlStructOf(*argument);
		std::string type = decl ? decl->name : std::string();
		if (!decl && argument->kind == ExpressionKind::Call && callsDependentTemplate(*argument)
			&& !deducedResultIsFundamental(*argument)) {
			// Its type is known only once deduced: that matters only if a function of this name is in a namespace.
			const bool elsewhere = std::any_of(_unit.helpers.begin(), _unit.helpers.end(), [&](const FunctionDecl& helper) {
					return helper.name.size() > last.size() + 2
						&& helper.name.compare(helper.name.size() - last.size() - 2, std::string::npos, "::" + last) == 0;
				}) || std::any_of(_unit.templates.begin(), _unit.templates.end(), [&](const FunctionTemplate& declaration) {
					return declaration.name.size() > last.size() + 2
						&& declaration.name.compare(declaration.name.size() - last.size() - 2, std::string::npos, "::" + last) == 0;
				});
			if (elsewhere) {
				throw CompileError("an unqualified call has an argument that is a call to a function template "
					"whose result type is known only once it is deduced, and argument-dependent lookup for \""
					+ last + "\" would search its namespace; assign that argument to a variable first");
			}
			continue;
		}
		if (!decl) {
			const auto tag = enumOf(*argument);
			if (argument->kind == ExpressionKind::Conditional && !tag) {
				throw CompileError("an unqualified call has a conditional expression as an argument, whose "
					"namespace argument-dependent lookup would search; that is not lowered yet");
			}
			type = tag && *tag != "<anonymous>" ? *tag : std::string();
		}
		if (type.empty()) {
			continue;
		}
		// A type of the file scope associates the file scope: its f is found from inside a namespace too.
		// A namespace that is inline associates its enclosing one, and a namespace its inline members.
		const size_t scope = type.rfind("::");
		std::vector<std::string> spaces { scope == std::string::npos ? std::string() : type.substr(0, scope) };
		for (size_t k = 0; k < spaces.size(); ++k) {
			const std::string current = spaces[k];
			const auto add = [&](const std::string& space) {
				if (std::find(spaces.begin(), spaces.end(), space) == spaces.end()) spaces.push_back(space);
			};
			if (_unit.inlineNamespaces.count(current)) {
				const size_t outer = current.rfind("::");
				add(outer == std::string::npos ? std::string() : current.substr(0, outer));
			}
			for (const std::string& inner: _unit.inlineNamespaces) {
				const size_t outer = inner.rfind("::");
				if ((outer == std::string::npos ? std::string() : inner.substr(0, outer)) == current) add(inner);
			}
		}
		for (const std::string& space: spaces) {
			const std::string associated = space.empty() ? last : space + "::" + last;
			const std::vector<HelperFunction*> found = helpersNamed(associated);
			const std::vector<const FunctionTemplate*> foundTemplates = templatesNamed(associated);
			// An instance is looked up again where clang instantiates it, at the end of the file.
			const bool anywhere = isInstance(_helper);
			const bool visible = anywhere ? !(found.empty() && foundTemplates.empty())
				: std::any_of(found.begin(), found.end(), [&](const HelperFunction* entry) {
					return entry->firstOrder <= callerOrder();
				}) || std::any_of(foundTemplates.begin(), foundTemplates.end(), [&](const FunctionTemplate* declaration) {
					return declaration->order <= callerOrder();
				});
			if (visible && adlOnly && anywhere) {
				adlOnly->insert(associated);
			}
			if (visible && std::find(names.begin(), names.end(), associated) == names.end()) {
				// The library's own overloads compete with it, which mslc does not model here.
				// (A struct argument matches no library overload, so there is nothing to compete with.)
				if (!decl && (isMSLBuiltinName(last) || findMathBuiltin(last) || findPackBuiltin(last))) {
					throw CompileError("argument-dependent lookup finds \"" + associated + "\", which competes with the "
						"standard library's \"" + last + "\"; Apple resolves the two by overload, which mslc "
						"does not model");
				}
				names.push_back(associated);
			}
		}
	}
	return names;
}

std::vector<Emitter::HelperFunction*> Emitter::helpersCalled(const Expression& call) {
	std::vector<HelperFunction*> all = helpersNamed(call.left->name);
	for (const std::string& name: call.left->overloadNames) {
		if (name != call.left->name) {
			const std::vector<HelperFunction*> named = helpersNamed(name);
			all.insert(all.end(), named.begin(), named.end());
		}
	}
	return all;
}

// Every function a name can denote: its one function, or each signature of an overloaded name.
std::vector<Emitter::HelperFunction*> Emitter::helpersNamed(const std::string& name) {
	if (const auto overloads = _overloads.find(name); overloads != _overloads.end()) {
		return overloads->second;
	}
	const auto found = _helpers.find(name);
	return found == _helpers.end() ? std::vector<HelperFunction*>() : std::vector<HelperFunction*> { &found->second };
}

std::vector<const FunctionTemplate*> Emitter::memberTemplatesOf(const std::string& owner, const std::string& member) const {
	return owner.empty() ? std::vector<const FunctionTemplate*>() : templatesNamed(owner + "::" + member);
}

std::vector<const FunctionTemplate*> Emitter::templatesNamed(const std::string& name) const {
	const auto found = _templatesByName.find(name);
	return found == _templatesByName.end() ? std::vector<const FunctionTemplate*>() : found->second;
}

// The templates a call's names denote that are declared above the call.
std::vector<const FunctionTemplate*> Emitter::visibleTemplates(const std::vector<std::string>& names,
	const std::set<std::string>& adl) const {
	std::vector<const FunctionTemplate*> visible;
	for (const std::string& name: names) {
		for (const FunctionTemplate* declaration: templatesNamed(name)) {
			if (declaration->order > callerOrder() && !(isInstance(_helper) && adl.count(name))) {
				continue;
			}
			if (_unit.findFunction(declaration->name)) {
				throw CompileError("\"" + declaration->name + "\" names both an entry point and a function template");
			}
			visible.push_back(declaration);
		}
	}
	return visible;
}

// What a call can denote before its arguments are known: the templates of the names the parser found.
std::vector<const FunctionTemplate*> Emitter::templatesCalled(const Expression& call) const {
	std::vector<const FunctionTemplate*> all = templatesNamed(call.left->name);
	for (const std::string& name: call.left->overloadNames) {
		if (name != call.left->name) {
			const std::vector<const FunctionTemplate*> named = templatesNamed(name);
			all.insert(all.end(), named.begin(), named.end());
		}
	}
	return all;
}

// The type a call's result has before it is emitted, for one aspect (its struct, its enum): the
// overloads of its arity have to agree. A template returning its own parameter has no struct or enum
// here until it is deduced.
const Type* Emitter::returningType(const Expression& call, std::string (*aspect)(const Type&)) {
	const Type* chosen = nullptr;
	const std::vector<HelperFunction*> named = helpersCalled(call);
	const std::vector<const FunctionTemplate*> templates = templatesCalled(call);
	const size_t total = named.size() + templates.size();
	const auto consider = [&](const Type& returned) {
		if (chosen) {
			if (aspect(*chosen) != aspect(returned)) {
				throw CompileError("the overloads of \"" + call.left->name + "\" differ in what they return "
					"in a way this use of the call depends on, and which one the call chooses is not known "
					"before its arguments are evaluated; assign the call to a variable first");
			}
			return;
		}
		chosen = &returned;
	};
	for (const HelperFunction* candidate: named) {
		const FunctionDecl& definition = *candidate->definition;
		if (total > 1
			&& (candidate->firstOrder > callerOrder() || definition.parameters.size() != call.arguments.size())) {
			continue;
		}
		consider(definition.returnType);
	}
	for (const FunctionTemplate* declaration: templates) {
		if (total > 1 && declaration->order > callerOrder()) {
			continue;
		}
		std::vector<TemplateArgument> arguments;
		const FunctionDecl* pattern = templatePattern(*declaration, call.left->templateArguments, arguments);
		if (!pattern || (total > 1 && pattern->parameters.size() != call.arguments.size())) {
			continue;
		}
		consider(pattern->returnType);
	}
	return chosen;
}

// The overload a call names, chosen by its arguments. Arity alone often decides; otherwise the
// arguments are evaluated here, once, left to right, and handed on in prepared so the call
// does not evaluate them again.


Emitter::HelperFunction* Emitter::resolveOverload(const Expression& call, const std::vector<HelperFunction*>& set,
	const std::vector<const FunctionTemplate*>& templates, const std::set<std::string>& adl,
	std::vector<Id>& prepared, const Expression* receiver) {
	// As written: Apple names the function as the call spells it.
	const size_t tail = call.left->name.rfind("::");
	const std::string name = receiver ? call.left->memberName
		: call.left->unqualifiedCall && tail != std::string::npos
		? call.left->name.substr(tail + 2) : call.left->name;
	// The receiver is the first argument of the template's signature, which is not in call.arguments.
	const size_t objects = receiver ? 1 : 0;
	const auto describe = [&](const std::vector<HelperFunction*>& entries) {
		std::string list;
		for (const HelperFunction* entry: entries) {
			list += (list.empty() ? "" : ", ") + std::string(isInstance(entry->definition) ? "template " : "")
				+ entry->definition->name + "(" + parameterSignature(*entry->definition) + ")";
		}
		return list;
	};
	const auto chosen = [&](HelperFunction* entry) {
		if (const auto instance = _instanceOf.find(entry->definition); instance != _instanceOf.end()) {
			commitInstance(*instance->second);
		}
		return definedHelper(entry);
	};
	// Explicit template arguments name a template, so a function is not a candidate.
	const bool onlyTemplates = call.left->hasTemplateArguments;
	std::vector<HelperFunction*> visible;
	std::vector<HelperFunction*> candidates;
	for (HelperFunction* entry: onlyTemplates ? std::vector<HelperFunction*>() : set) {
		if (entry->firstOrder > callerOrder() && !(isInstance(_helper) && adl.count(entry->definition->name))) {
			continue;
		}
		visible.push_back(entry);
		if (entry->definition->parameters.size() == call.arguments.size() + objects) {
			candidates.push_back(entry);
		}
	}
	// The templates whose signature takes this many arguments.
	struct TemplateCandidate {
		const FunctionTemplate* declaration;
		const FunctionDecl* pattern;
		size_t given;
		std::vector<TemplateArgument> arguments;
	};
	std::vector<TemplateCandidate> patterns;
	std::string templateList;
	for (const FunctionTemplate* declaration: templates) {
		TemplateCandidate candidate { declaration, nullptr, call.left->templateArguments.size(), {} };
		candidate.pattern = templatePattern(*declaration, call.left->templateArguments, candidate.arguments);
		if (!candidate.pattern) {
			continue;
		}
		templateList += (templateList.empty() ? "" : ", ") + describeTemplate(*declaration, *candidate.pattern);
		if (candidate.pattern->parameters.size() == call.arguments.size() + objects) {
			patterns.push_back(std::move(candidate));
		}
	}
	if (visible.empty() && templates.empty() && !onlyTemplates) {
		throw CompileError("\"" + name + "\" is called before it is declared; declare it above its first use");
	}
	if (candidates.empty() && patterns.empty()) {
		throw CompileError("no matching function for call to \"" + name + "\": it is called with "
			+ std::to_string(call.arguments.size()) + " arguments; candidates: "
			+ (describe(visible) + (visible.empty() || templateList.empty() ? "" : ", ") + templateList));
	}
	if (candidates.size() == 1 && patterns.empty()) {
		return chosen(candidates.front());
	}

	// A conditional or a comma with a place for an operand may be an lvalue, in any address space; the
	// ranking below takes every one for a thread prvalue and could pick the wrong function. One made of
	// values only is a prvalue.
	const auto placeLike = [this](const auto& self, const Expression& e) -> bool {
		switch (e.kind) {
			case ExpressionKind::Identifier: case ExpressionKind::Index: case ExpressionKind::Member: return !isComputedValue(e);
			case ExpressionKind::Comma: return self(self, *e.right);
			// One place arm is enough to refuse: Apple binds `c ? la : 2.0f` to a non-const reference
			// parameter (measured), so it is not a plain prvalue.
			case ExpressionKind::Conditional:
				return self(self, *e.arguments.at(0)) || self(self, *e.arguments.at(1));
			default: return false;
		}
	};
	for (const ExpressionPtr& argument: call.arguments) {
		if ((argument->kind == ExpressionKind::Conditional || argument->kind == ExpressionKind::Comma)
			&& placeLike(placeLike, *argument)) {
			throw CompileError("a conditional or comma expression as an argument chooses among the overloads of \""
				+ name + "\", which is not lowered yet (whether it is an lvalue, and in which address space, "
				"decides the overload); assign it to a variable first");
		}
	}
	std::vector<OverloadArgument> arguments;
	if (receiver) {
		OverloadArgument object;
		object.expression = receiver;
		object.value = _pendingObject;
		arguments.push_back(std::move(object));
	}
	for (const ExpressionPtr& argument: call.arguments) {
		arguments.push_back(classifyArgument(*argument));
	}
	// A template's signature before deduction still says what kind of argument each position takes.
	std::vector<const std::vector<Parameter>*> lists;
	for (const HelperFunction* entry: candidates) {
		lists.push_back(&entry->definition->parameters);
	}
	for (const TemplateCandidate& candidate: patterns) {
		lists.push_back(&candidate.pattern->parameters);
	}
	bool enumParameter = false;
	for (const std::vector<Parameter>* list: lists) {
		for (const Parameter& parameter: *list) {
			enumParameter = enumParameter || !parameter.type.enumName.empty();
			if (parameter.type.enumName == "<anonymous>") {
				throw CompileError("an overload of \"" + name + "\" takes a parameter of an anonymous enum, "
					"which cannot be told from another anonymous enum here; that is not lowered yet");
			}
		}
	}
	for (size_t i = objects; i < arguments.size(); ++i) {
		OverloadArgument& argument = arguments[i];
		if (argument.kind == OverloadArgument::Kind::Array || argument.kind == OverloadArgument::Kind::Resource
			|| argument.pointerLike) {
			continue;
		}
		const bool bound = std::any_of(lists.begin(), lists.end(), [&](const std::vector<Parameter>* list) {
			return (*list)[i].isMutableReference();
		});
		if (bound && argument.kind == OverloadArgument::Kind::Variable && !isPlainVariable(*argument.expression)) {
			throw CompileError("argument " + std::to_string(i + 1) + " of the call to \"" + name
				+ "\" is evaluated to choose among overloads and is also bound to a reference by one; "
				"name a local variable instead");
		}
		// Whether an argument is an enum decides its rank, and enumOf only knows what it can see.
		argument.tag = enumOf(*argument.expression);
		if (enumParameter && (!argument.tag || *argument.tag == "<anonymous>")) {
			throw CompileError("argument " + std::to_string(i + 1) + " of the call to \"" + name
				+ "\" is not known to be a particular enum, and an overload takes an enum; that is not "
				"lowered yet");
		}
		// (A device or constant lvalue never binds a thread reference: that candidate just drops out.)
		if (bound && argument.kind == OverloadArgument::Kind::Variable && argument.inThread
			&& !isWholeVariable(*argument.expression)) {
			throw CompileError("argument " + std::to_string(i + 1) + " of the call to \"" + name
				+ "\" is an element or member that a mutable reference could bind, which is not lowered "
				"when choosing among overloads (a vector component cannot be bound); name a local instead");
		}
		argument.value = emitExpression(*argument.expression);
		prepared.push_back(argument.value);
	}
	// Positions that are not evaluated keep their place, so prepared[i] is argument i.
	if (!prepared.empty() && prepared.size() != arguments.size() - objects) {
		prepared.clear();
		for (size_t i = objects; i < arguments.size(); ++i) {
			prepared.push_back(arguments[i].value);
		}
	}

	// Whether the second template's parameters can all be read off the first's, by value types, pointers,
	// class template instances and consistent markers; anything else (arrays) is left unordered.
	const auto deducibleFrom = [&](const TemplateCandidate& from, const TemplateCandidate& to) {
		if (from.pattern->parameters.size() != to.pattern->parameters.size()) {
			return false;
		}
		const auto isParameter = [](const FunctionTemplate& declaration, const Type& type) {
			return isTemplateMarker(type) && std::any_of(declaration.parameters.begin(), declaration.parameters.end(),
				[&](const TemplateParameter& p) { return p.isType && templateMarker(p) == type.namedType; });
		};
		const auto isInstance = [](const FunctionDecl& decl, const Type& type) {
			return decl.dependentInstances.count(type.namedType) > 0;
		};
		// A parameter of the second template takes the first's type for it; asked again, it must agree.
		std::map<std::string, Type> bound;
		const auto bind = [&](const std::string& marker, const Type& type) {
			const auto [slot, fresh] = bound.emplace(marker, type);
			return fresh || (slot->second.namedType == type.namedType
				&& sameValueType(slot->second, type) && slot->second.isConst == type.isConst
				&& slot->second.isPointer == type.isPointer
				&& slot->second.addressSpace == type.addressSpace);
		};
		const auto matchInstance = [&](const auto& self, const Type& a, const Type& b) -> bool {
			const DependentInstance& have = from.pattern->dependentInstances.at(a.namedType);
			const DependentInstance& want = to.pattern->dependentInstances.at(b.namedType);
			if (have.classTemplate != want.classTemplate || have.arguments.size() != want.arguments.size()) {
				return false;
			}
			for (size_t k = 0; k < want.arguments.size(); ++k) {
				const TemplateArgument& x = have.arguments[k];
				const TemplateArgument& y = want.arguments[k];
				if (x.isType != y.isType) {
					return false;
				}
				if (!y.isType) {
					if (x.value != y.value) {
						return false;
					}
				} else if (isInstance(*to.pattern, y.type)) {
					if (!isInstance(*from.pattern, x.type) || x.type.isConst != y.type.isConst
						|| !self(self, x.type, y.type)) {
						return false;
					}
				} else if (isParameter(*to.declaration, y.type)) {
					if ((y.type.isConst && !x.type.isConst) || !bind(y.type.namedType, [&] {
							Type t = x.type;
							t.isConst = x.type.isConst && !y.type.isConst;
							return t;
						}())) {
						return false;
					}
				} else if (isParameter(*from.declaration, x.type) || isInstance(*from.pattern, x.type)
					|| !sameValueType(x.type, y.type) || x.type.isConst != y.type.isConst) {
					return false;
				}
			}
			return true;
		};
		for (size_t i = 0; i < to.pattern->parameters.size(); ++i) {
			const Parameter& a = from.pattern->parameters[i];
			const Parameter& b = to.pattern->parameters[i];
			const auto plain = [](const Parameter& p) {
				return !p.type.isPointer && !p.type.arrayLength && p.type.resource == ResourceKind::None;
			};
			const auto pointer = [](const Parameter& p) {
				return p.type.isPointer && !p.isReference && !p.type.arrayLength
					&& p.type.innerArrayLengths.empty() && p.type.resource == ResourceKind::None;
			};
			if (pointer(a) && pointer(b)) {
				if (a.type.addressSpace != b.type.addressSpace || (b.type.isConst && !a.type.isConst)
					|| isInstance(*from.pattern, a.type) || isInstance(*to.pattern, b.type)) {
					return false;
				}
				Type pointee = a.type;
				pointee.isPointer = false;
				pointee.addressSpace = AddressSpace::None;
				pointee.isConst = a.type.isConst && !b.type.isConst;
				if (isParameter(*to.declaration, b.type)) {
					if (!bind(b.type.namedType, pointee)) {
						return false;
					}
				} else if (isParameter(*from.declaration, a.type) || a.type.isConst != b.type.isConst
					|| !sameValueType(a.type, b.type)) {
					return false;
				}
				continue;
			}
			if (pointer(a) && plain(b) && (!b.isReference || (b.type.isConst && b.type.addressSpace == AddressSpace::Thread))
				&& isParameter(*to.declaration, b.type) && !isInstance(*from.pattern, a.type)) {
				if (!bind(b.type.namedType, a.type)) {
					return false;
				}
				continue;
			}
			if (!plain(a) || !plain(b) || a.isReference != b.isReference || a.type.isConst != b.type.isConst) {
				return false;
			}
			if (isInstance(*to.pattern, b.type)) {
				if (!isInstance(*from.pattern, a.type) || !matchInstance(matchInstance, a.type, b.type)) {
					return false;
				}
			} else if (isParameter(*to.declaration, b.type)) {
				if (!bind(b.type.namedType, a.type)) {
					return false;
				}
			} else if (isInstance(*from.pattern, a.type) || isParameter(*from.declaration, a.type)
				|| !sameValueType(a.type, b.type)) {
				return false;
			}
		}
		return true;
	};

	// A template competes through its instance; one that cannot be deduced is no candidate (as in C++).
	std::map<const HelperFunction*, const TemplateCandidate*> patternOf;
	struct DeferredPointerTemplate {
		const TemplateCandidate* candidate;
		std::vector<TemplateArgument> deduced;
		size_t pointerArgIndex;
	};
	std::vector<DeferredPointerTemplate> pointerValueCandidates;
	for (const TemplateCandidate& candidate: patterns) {
		std::vector<TemplateArgument> deduced = candidate.arguments;
		if (!deduceTemplateArguments(*candidate.declaration, *candidate.pattern, candidate.given, arguments, deduced)) {
			continue;
		}
		bool pointerInValueParameter = false;
		size_t pointerArgIndex = 0;
		for (size_t i = 0; i < candidate.pattern->parameters.size(); ++i) {
			const Parameter& p = candidate.pattern->parameters[i];
			if (p.type.isPointer || !isTemplateMarker(p.type)
				|| candidate.pattern->dependentInstances.count(p.type.namedType)) {
				continue;
			}
			const OverloadArgument& arg = arguments[i];
			const bool buffer = arg.kind == OverloadArgument::Kind::Variable && !arg.expression->fileScope
				&& arg.binding && arg.binding->isPointer && arg.binding->isBuffer
				&& arg.binding->bufferPointeeType != InvalidId;
			if (arg.kind == OverloadArgument::Kind::Array || buffer) {
				pointerInValueParameter = true;
				pointerArgIndex = i;
				break;
			}
		}
		if (pointerInValueParameter) {
			pointerValueCandidates.push_back({ &candidate, std::move(deduced), pointerArgIndex });
			continue;
		}
		if (candidate.declaration->constructorMember && arguments.size() == 1) {
			TypeTable::StructForm argForm;
			if (arguments[0].kind != OverloadArgument::Kind::Array
				&& arguments[0].kind != OverloadArgument::Kind::Resource && !arguments[0].pointerLike
				&& _types.structForm(_builder.typeOf(arguments[0].value), argForm)
				&& argForm.name == candidate.declaration->memberOf) {
				throw CompileError("copy construction of \"" + candidate.declaration->memberOf
					+ "\" with parentheses when it declares a constructor template is not lowered yet; "
					"copy it with \"=\" instead");
			}
		}
		try {
			HelperFunction* made = &templateInstance(*candidate.declaration, std::move(deduced)).helper;
			candidates.push_back(made);
			patternOf[made] = &candidate;
		} catch (const SubstitutionFailure&) {
			// The signature with these arguments is ill-formed (an enable_if): not a candidate.
		}
	}

	std::vector<std::pair<HelperFunction*, std::vector<ConversionCost>>> viable;
	for (HelperFunction* entry: candidates) {
		std::vector<ConversionCost> costs;
		for (size_t i = 0; i < arguments.size(); ++i) {
			// Every candidate takes the one receiver, so it cannot rank them.
			costs.push_back(i < objects ? ConversionCost { kExact, -1 }
				: conversionCost(arguments[i], entry->definition->parameters[i]));
		}
		if (std::none_of(costs.begin(), costs.end(), [](const ConversionCost& cost) { return cost.rank == kNoConversion; })) {
			viable.emplace_back(entry, std::move(costs));
		}
	}
	// A plain T parameter that would have deduced a pointer from a buffer or local array cannot be
	// instantiated here, so it is dropped only when a viable non-template or more specialized viable
	// template beats it across every argument (including conversion and const qualification).
	const auto costsBeat = [](const std::vector<ConversionCost>& a, const std::vector<ConversionCost>& b,
		bool& strictlyBetter) {
		strictlyBetter = false;
		for (size_t i = 0; i < a.size(); ++i) {
			const bool ranked = a[i].rank != b[i].rank;
			const bool qualified = !ranked && a[i].qualification >= 0 && b[i].qualification >= 0
				&& a[i].qualification != b[i].qualification;
			if (ranked ? a[i].rank > b[i].rank : qualified && a[i].qualification > b[i].qualification) {
				return false;
			}
			strictlyBetter = strictlyBetter || (ranked ? a[i].rank < b[i].rank : qualified);
		}
		return true;
	};
	for (const DeferredPointerTemplate& deferred: pointerValueCandidates) {
		const TemplateCandidate* candidate = deferred.candidate;
		std::vector<ConversionCost> deferredCosts;
		bool deferredViable = true;
		for (size_t i = 0; i < arguments.size(); ++i) {
			if (i < objects) {
				deferredCosts.push_back({ kExact, -1 });
				continue;
			}
			Parameter sub = candidate->pattern->parameters[i];
			const OverloadArgument& arg = arguments[i];
			if (candidate->pattern->dependentInstances.count(sub.type.namedType)) {
				TypeTable::StructForm form;
				if (_types.structForm(_builder.typeOf(arg.value), form)) {
					sub.type.namedType = form.name;
				}
			} else {
				for (size_t j = 0; j < candidate->declaration->parameters.size(); ++j) {
					if (candidate->declaration->parameters[j].isType
						&& sub.type.namedType == templateMarker(candidate->declaration->parameters[j])) {
						const bool wasPointer = sub.type.isPointer;
						const bool wasConst = sub.type.isConst;
						const AddressSpace wasSpace = sub.type.addressSpace;
						const std::vector<uint32_t> wasInner = sub.type.innerArrayLengths;
						sub.type = deferred.deduced[j].type;
						sub.type.isPointer = sub.type.isPointer || wasPointer;
						sub.type.isConst = sub.type.isConst || wasConst;
						if (wasSpace != AddressSpace::None) {
							sub.type.addressSpace = wasSpace;
						}
						if (!wasInner.empty()) {
							sub.type.innerArrayLengths = wasInner;
						}
						break;
					}
				}
			}
			const bool buffer = arg.kind == OverloadArgument::Kind::Variable && !arg.expression->fileScope
				&& arg.binding && arg.binding->isPointer && arg.binding->isBuffer
				&& arg.binding->bufferPointeeType != InvalidId;
			if ((arg.kind == OverloadArgument::Kind::Array || buffer) && !candidate->pattern->parameters[i].type.isPointer) {
				deferredCosts.push_back({ kExact, 0 });
			} else {
				const ConversionCost cost = conversionCost(arg, sub);
				if (cost.rank == kNoConversion) {
					deferredViable = false;
					break;
				}
				deferredCosts.push_back(cost);
			}
		}
		if (!deferredViable) {
			continue;
		}
		const bool beaten = std::any_of(viable.begin(), viable.end(), [&](const auto& entry) {
			bool better = false;
			if (!costsBeat(entry.second, deferredCosts, better)) {
				return false;
			}
			if (better || !isInstance(entry.first->definition)) {
				return true;
			}
			const auto other = patternOf.find(entry.first);
			return other != patternOf.end() && deducibleFrom(*other->second, *candidate)
				&& !deducibleFrom(*candidate, *other->second);
		});
		if (!beaten) {
			throw CompileError("the type parameter of \"" + candidate->declaration->name
				+ "\" is deduced from argument " + std::to_string(deferred.pointerArgIndex + 1)
				+ ", which is an array, a pointer or a texture; that is not lowered yet");
		}
	}
	if (viable.empty()) {
		throw CompileError("no matching function for call to \"" + name + "\"; candidates: "
			+ describe(candidates) + (templateList.empty() ? "" : "; templates: " + templateList));
	}

	const auto moreSpecialized = [&](const HelperFunction* first, const HelperFunction* second) {
		const auto a = patternOf.find(first);
		const auto b = patternOf.find(second);
		return a != patternOf.end() && b != patternOf.end() && deducibleFrom(*a->second, *b->second)
			&& !deducibleFrom(*b->second, *a->second);
	};

	// One wins when no worse on any argument and better on one, or equal and a function beside a template.
	const auto beats = [&](const auto& first, const auto& second) {
		const std::vector<ConversionCost>& a = first.second;
		const std::vector<ConversionCost>& b = second.second;
		bool better = false;
		for (size_t i = 0; i < a.size(); ++i) {
			const bool ranked = a[i].rank != b[i].rank;
			const bool qualified = !ranked && a[i].qualification >= 0 && b[i].qualification >= 0
				&& a[i].qualification != b[i].qualification;
			if (ranked ? a[i].rank > b[i].rank : qualified && a[i].qualification > b[i].qualification) {
				return false;
			}
			better = better || (ranked ? a[i].rank < b[i].rank : qualified);
		}
		return better || (!isInstance(first.first->definition) && isInstance(second.first->definition))
			|| moreSpecialized(first.first, second.first);
	};
	for (const auto& winner: viable) {
		if (std::all_of(viable.begin(), viable.end(), [&](const auto& other) {
			return other.first == winner.first || beats(winner, other);
		})) {
			return chosen(winner.first);
		}
	}
	std::vector<HelperFunction*> tied;
	for (const auto& candidate: viable) {
		tied.push_back(candidate.first);
	}
	throw CompileError("call to \"" + name + "\" is ambiguous; candidates: " + describe(tied));
}

} // namespace mslc
