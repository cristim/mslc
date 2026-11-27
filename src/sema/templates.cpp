#include "sema/emitter.h"

namespace mslc {

// The type a template parameter stands for while its signature is read to deduce it.
std::string templateMarker(const TemplateParameter& parameter) {
	return "<" + parameter.name + ">";
}

bool isTemplateMarker(const Type& type) {
	return type.namedType.size() > 2 && type.namedType.front() == '<' && type.namedType.back() == '>';
}

// The argument as a name that tells instances apart.
std::string templateArgumentName(const TemplateArgument& argument) {
	if (argument.unbound) {
		return "?";
	}
	if (!argument.isType) {
		return std::to_string(argument.value);
	}
	Type bare = argument.type;
	bare.addressSpace = AddressSpace::None;
	return typeName(bare);
}

// Whether a constant fits the parameter's type (C++ rejects a narrowing one).
bool fitsTemplateConstant(ScalarKind kind, int64_t value) {
	switch (kind) {
		case ScalarKind::Bool: return value == 0 || value == 1;
		case ScalarKind::Char: return value >= -128 && value <= 127;
		case ScalarKind::UChar: return value >= 0 && value <= 255;
		case ScalarKind::Short: return value >= -32768 && value <= 32767;
		case ScalarKind::UShort: return value >= 0 && value <= 65535;
		case ScalarKind::Int: return value >= INT32_MIN && value <= INT32_MAX;
		case ScalarKind::UInt: return value >= 0 && value <= static_cast<int64_t>(UINT32_MAX);
		case ScalarKind::ULong: return value >= 0;
		default: return true;
	}
}

bool sameValueType(const Type& a, const Type& b) {
	return a.scalar == b.scalar && a.vectorWidth == b.vectorWidth && a.matrixColumns == b.matrixColumns
		&& a.isPacked == b.isPacked && a.namedType == b.namedType && a.enumName == b.enumName
		&& a.isAtomic == b.isAtomic;
}

// A template's signature read with the explicit arguments, the type parameters to deduce standing
// for themselves (a marker type) and a constant neither given nor defaulted unknown. Null when the
// explicit arguments do not fit, which makes it no candidate.
const FunctionDecl* Emitter::templatePattern(const FunctionTemplate& declaration,
	const std::vector<TemplateArgument>& given, std::vector<TemplateArgument>& arguments) {
	arguments.clear();
	if (given.size() > declaration.parameters.size()) {
		return nullptr;
	}
	for (size_t i = 0; i < declaration.parameters.size(); ++i) {
		const TemplateParameter& parameter = declaration.parameters[i];
		TemplateArgument argument;
		if (i < given.size()) {
			argument = given[i];
			if (argument.isType != parameter.isType
				|| (!parameter.isType && !fitsTemplateConstant(parameter.kind, argument.value))) {
				return nullptr;
			}
		} else if (parameter.isType) {
			argument.type.namedType = templateMarker(parameter);
		} else if (parameter.hasDefault) {
			try {
				argument = templateDefault(declaration, i, arguments);
			} catch (const SubstitutionFailure&) {
				return nullptr;
			}
			if (!fitsTemplateConstant(parameter.kind, argument.value)) {
				return nullptr;
			}
		} else {
			argument.isType = false;
			argument.unbound = true;
		}
		arguments.push_back(std::move(argument));
	}

	std::string key = std::to_string(&declaration - _unit.templates.data()) + "<";
	for (const TemplateArgument& argument: arguments) {
		key += templateArgumentName(argument) + ",";
	}
	std::unique_ptr<FunctionDecl>& slot = _patterns[key];
	if (!slot) {
		try {
			slot = std::make_unique<FunctionDecl>(instantiateTemplate(declaration, arguments, false));
		} catch (const SubstitutionFailure&) {
			_patterns.erase(key);
			return nullptr;
		}
	}
	return slot.get();
}

// Fills the type parameters left, from the argument types: T takes the argument's type by value or
// reference, T* a local array's element type and vec<T, N> a vector's component type; T used twice
// takes one type or the template is no candidate. What C++ deduces and mslc does not model is an
// error, never a template dropped, which could let another function win.
bool Emitter::deduceTemplateArguments(const FunctionTemplate& declaration, const FunctionDecl& pattern,
	size_t given, const std::vector<OverloadArgument>& arguments, std::vector<TemplateArgument>& deduced) {
	const auto parameterIndex = [&](const Type& type) {
		for (size_t j = given; j < declaration.parameters.size(); ++j) {
			if (declaration.parameters[j].isType && isTemplateMarker(type)
				&& type.namedType == templateMarker(declaration.parameters[j])) {
				return j;
			}
		}
		return declaration.parameters.size();
	};
	const auto refuse = [&](size_t position, const std::string& why) {
		return CompileError("the type parameter of \"" + declaration.name + "\" is deduced from argument "
			+ std::to_string(position + 1) + ", " + why + "; that is not lowered yet");
	};
	std::vector<bool> known(deduced.size(), false);
	const auto bind = [&](size_t index, const Type& value) {
		if (known[index]) {
			return sameValueType(deduced[index].type, value) && deduced[index].type.isConst == value.isConst;
		}
		known[index] = true;
		deduced[index].type = value;
		return true;
	};
	// A struct is an instance of the class template a parameter names, with arguments that agree: each
	// type parameter in it takes the instance's argument. A class template never converts to another, so
	// a mismatch is no candidate, as in C++.
	const auto matchInstance = [&](const auto& self, const std::string& marker, const std::string& structName) -> bool {
		const DependentInstance& expected = pattern.dependentInstances.at(marker);
		const StructDecl* made = structName.empty() ? nullptr : _unit.findStruct(structName);
		if (!made || made->classTemplate != expected.classTemplate
			|| made->classArguments.size() != expected.arguments.size()) {
			return false;
		}
		for (size_t k = 0; k < expected.arguments.size(); ++k) {
			const TemplateArgument& want = expected.arguments[k];
			const TemplateArgument& got = made->classArguments[k];
			if (want.isType != got.isType) {
				return false;
			}
			if (!want.isType) {
				if (want.value != got.value) {
					return false;
				}
			} else if (isTemplateMarker(want.type)) {
				if (!pattern.dependentInstances.count(want.type.namedType)) {
					const size_t slot = parameterIndex(want.type);
					if (slot == declaration.parameters.size()) {
						throw CompileError("the signature of \"" + declaration.name + "\" names a type parameter "
							"that is neither deduced nor given; that is not lowered yet");
					}
					// A qualifier the pattern spells has to be the argument's, and is not part of T.
					Type bound = got.type;
					if (want.type.isConst) {
						if (!got.type.isConst) {
							return false;
						}
						bound.isConst = false;
					}
					if (!bind(slot, bound)) {
						return false;
					}
				} else if ((want.type.isConst && !got.type.isConst) || !self(self, want.type.namedType, got.type.isPointer || got.type.arrayLength
						? std::string() : got.type.namedType)) {
					return false;
				}
			} else if (!sameValueType(want.type, got.type) || want.type.isConst != got.type.isConst) {
				return false;
			}
		}
		return true;
	};
	for (size_t i = 0; i < pattern.parameters.size(); ++i) {
		const Parameter& parameter = pattern.parameters[i];
		if (pattern.dependentInstances.count(parameter.type.namedType)) {
			if (parameter.type.isPointer || parameter.type.arrayLength || parameter.type.resource != ResourceKind::None) {
				throw refuse(i, "to a parameter that is an array or a pointer to an instance of a class template");
			}
			const OverloadArgument& argument = arguments[i];
			TypeTable::StructForm form;
			if (argument.kind == OverloadArgument::Kind::Array || argument.kind == OverloadArgument::Kind::Resource
				|| argument.pointerLike || !_types.structForm(_builder.typeOf(argument.value), form)
				|| !matchInstance(matchInstance, parameter.type.namedType, form.name)) {
				return false;
			}
			continue;
		}
		const size_t index = parameterIndex(parameter.type);
		if (index == declaration.parameters.size()) {
			continue;
		}
		const OverloadArgument& argument = arguments[i];
		Type value;
		if (parameter.type.isPointer) {
			const bool buffer = argument.kind == OverloadArgument::Kind::Variable && !argument.expression->fileScope
				&& argument.binding && argument.binding->isPointer && argument.binding->isBuffer
				&& argument.binding->bufferPointeeType != InvalidId;
			if (argument.kind != OverloadArgument::Kind::Array && !buffer) {
				if (argument.pointerLike || argument.kind == OverloadArgument::Kind::Resource) {
					throw refuse(i, "which is a pointer or a texture and neither a local array nor a whole buffer");
				}
				return false;
			}
			const AddressSpace arraySpace = argument.binding->storageClass == spirv::StorageClass::Workgroup
				? AddressSpace::Threadgroup : AddressSpace::Thread;
			if ((buffer ? argument.binding->pointeeMsl.addressSpace : arraySpace) != parameter.type.addressSpace) {
				return false;
			}
			value = argument.binding->pointeeMsl;
			if (!buffer) {
				value.dropOuterDimension();
			}
			if (value.arrayLength || value.isPointer || value.resource != ResourceKind::None) {
				throw refuse(i, "which is an array of arrays");
			}
			if (value.isAtomic || !value.enumName.empty()) {
				throw refuse(i, "which points at an atomic or an enum");
			}
			// thread vec<T, N>* takes T from an array of vectors of N components.
			if (parameter.type.isVector()) {
				if (value.vectorWidth != parameter.type.vectorWidth || value.isMatrix() || value.isPacked
					|| !value.namedType.empty() || !value.enumName.empty()) {
					return false;
				}
				value.vectorWidth = 0;
			}
			value.addressSpace = AddressSpace::None;
			value.isConst = (argument.binding->readOnly && parameter.type.addressSpace != AddressSpace::Constant)
				&& !parameter.type.isConst;
		} else if (parameter.type.isTexture() && !parameter.type.arrayLength) {
			// texture2d<T> takes T from a texture of the same kind, depth, multisampling and access.
			if (argument.kind != OverloadArgument::Kind::Resource) {
				return false;
			}
			const Type& texture = argument.binding->pointeeMsl;
			if (texture.resource != parameter.type.resource || texture.isDepth != parameter.type.isDepth
				|| texture.isMultisampled != parameter.type.isMultisampled
				|| texture.textureAccess != parameter.type.textureAccess || texture.arrayLength) {
				return false;
			}
			value = Type();
			value.scalar = texture.scalar;
		} else {
			if (parameter.type.arrayLength || parameter.type.resource != ResourceKind::None) {
				throw refuse(i, "to a parameter that is an array or a texture");
			}
			// A T taken by value from a texture or a sampler is that texture's or sampler's type.
			if (argument.kind == OverloadArgument::Kind::Resource && !parameter.isReference
				&& !parameter.type.isVector() && !argument.binding->pointeeMsl.arrayLength) {
				value = argument.binding->pointeeMsl;
				value.isConst = false;
				value.addressSpace = AddressSpace::None;
				if (known[index]) {
					if (!sameResourceType(deduced[index].type, value) || !deduced[index].type.isTexture() != !value.isTexture()) {
						return false;
					}
					continue;
				}
				known[index] = true;
				deduced[index].type = value;
				continue;
			}
			const bool buffer = argument.kind == OverloadArgument::Kind::Variable && !argument.expression->fileScope
				&& argument.binding && argument.binding->isPointer && argument.binding->isBuffer
				&& argument.binding->bufferPointeeType != InvalidId;
			if ((argument.kind == OverloadArgument::Kind::Array || buffer)
				&& (!parameter.isReference || (parameter.type.isConst && parameter.type.addressSpace == AddressSpace::Thread))
				&& !parameter.type.isVector()) {
				value = argument.binding->pointeeMsl;
				if (!buffer) {
					value.dropOuterDimension();
				}
				if (value.arrayLength || value.isPointer || value.resource != ResourceKind::None) {
					throw refuse(i, "which is an array of arrays");
				}
				if (value.isAtomic || !value.enumName.empty()) {
					throw refuse(i, "which points at an atomic or an enum");
				}
				value.isPointer = true;
				const AddressSpace arraySpace = argument.binding->storageClass == spirv::StorageClass::Workgroup
					? AddressSpace::Threadgroup : AddressSpace::Thread;
				value.addressSpace = buffer ? argument.binding->pointeeMsl.addressSpace : arraySpace;
				value.isConst = argument.binding->readOnly && value.addressSpace != AddressSpace::Constant;
			} else {
				if (argument.kind == OverloadArgument::Kind::Array || argument.kind == OverloadArgument::Kind::Resource
					|| argument.pointerLike) {
					throw refuse(i, "which is an array, a pointer or a texture");
				}
				if (argument.tag && !argument.tag->empty()) {
					throw refuse(i, "which is an enum");
				}
				// Apple deduces T = "device float" for such an lvalue and binds it, as a plain float& cannot.
				if (parameter.isReference && argument.kind == OverloadArgument::Kind::Variable && !argument.inThread) {
					throw refuse(i, "which is an lvalue in device, constant or threadgroup memory, where Apple "
						"deduces an address-space-qualified type");
				}
				value = deduceAutoType(Type(), _builder.typeOf(argument.value), "argument " + std::to_string(i + 1));
				// vec<T, N> takes T from a vector of N components and fails to deduce from anything else.
				if (parameter.type.isVector()) {
					if (value.vectorWidth != parameter.type.vectorWidth || value.isMatrix() || value.isPacked
						|| !value.namedType.empty() || !value.enumName.empty()) {
						return false;
					}
					value.vectorWidth = 0;
				}
				// A mutable reference to a const object has T = "const float".
				value.isConst = parameter.isMutableReference() && argument.kind == OverloadArgument::Kind::Variable
					&& argument.inThread && !argument.changeable;
			}
		}
		if (known[index]) {
			if (!sameValueType(deduced[index].type, value) || deduced[index].type.isConst != value.isConst
				|| deduced[index].type.isPointer != value.isPointer
				|| deduced[index].type.addressSpace != value.addressSpace) {
				return false;
			}
			continue;
		}
		known[index] = true;
		deduced[index].type = value;
	}

	for (size_t j = given; j < declaration.parameters.size(); ++j) {
		const TemplateParameter& parameter = declaration.parameters[j];
		if (parameter.isType && !known[j]) {
			if (!parameter.hasDefault) {
				return false;
			}
			try {
				deduced[j] = templateDefault(declaration, j, deduced);
			} catch (const SubstitutionFailure&) {
				return false;
			}
		} else if (!parameter.isType && deduced[j].unbound) {
			return false;
		}
	}
	return true;
}

std::string Emitter::instanceKey(const FunctionTemplate& declaration,
	const std::vector<TemplateArgument>& arguments) const {
	std::string key = std::to_string(&declaration - _unit.templates.data()) + "<";
	for (const TemplateArgument& argument: arguments) {
		key += templateArgumentName(argument) + ",";
	}
	return key;
}

// Which instance each explicit specialization is the body of. Its arguments are the explicit ones it
// names and, for the rest, the types its parameters have where the primary template's name a type
// parameter, as C++ deduces them from the specialization's own declaration ([temp.expl.spec],
// [temp.deduct.decl]); defaults fill what is left. It specializes the primary templates of its name
// declared above it whose instance for those arguments has its return and parameter types: exactly
// one has to, or it is refused, as is a second specialization of one instance.
void Emitter::resolveSpecializations() {
	for (const ExplicitSpecialization& specialization: _unit.specializations) {
		const FunctionTemplate& written = specialization.declaration;
		const FunctionDecl signature = instantiateTemplate(written, {}, false);
		// The function types are compared as C++ does: a by-value parameter's top-level const is not part of it,
		// and the address space and enum are, which typeName does not spell.
		const auto sameParameter = [](const Parameter& a, const Parameter& b) {
			Type left = a.type;
			Type right = b.type;
			if (!a.isReference && !left.isPointer) {
				left.isConst = false;
			}
			if (!b.isReference && !right.isPointer) {
				right.isConst = false;
			}
			return a.isReference == b.isReference && typeName(left) == typeName(right)
				&& left.addressSpace == right.addressSpace && left.enumName == right.enumName;
		};
		std::vector<std::pair<const FunctionTemplate*, std::vector<TemplateArgument>>> matches;
		const auto found = _templatesByName.find(written.name);
		for (const FunctionTemplate* primary: found == _templatesByName.end()
				? std::vector<const FunctionTemplate*>() : found->second) {
			if (primary->order > written.order || !primary->memberOf.empty()) {
				continue;
			}
			std::vector<TemplateArgument> arguments;
			const FunctionDecl* pattern = templatePattern(*primary, specialization.given, arguments);
			if (!pattern || pattern->parameters.size() != signature.parameters.size()) {
				continue;
			}
			std::vector<bool> known(arguments.size(), false);
			for (size_t j = 0; j < specialization.given.size(); ++j) {
				known[j] = true;
			}
			bool fits = true;
			for (size_t i = 0; i < pattern->parameters.size() && fits; ++i) {
				const Type& wanted = pattern->parameters[i].type;
				if (!isTemplateMarker(wanted) || wanted.isPointer || wanted.arrayLength) {
					continue;
				}
				for (size_t j = 0; j < primary->parameters.size(); ++j) {
					if (!primary->parameters[j].isType || wanted.namedType != templateMarker(primary->parameters[j])) {
						continue;
					}
					Type value = signature.parameters[i].type;
					value.isConst = false;
					value.addressSpace = AddressSpace::None;
					// vec<T, N> takes T from a vector of N components, as a call deduces it (deduceTemplateArguments).
					if (wanted.isVector()) {
						if (value.vectorWidth != wanted.vectorWidth || value.isMatrix() || value.isPacked
							|| !value.namedType.empty() || !value.enumName.empty()) {
							fits = false;
							break;
						}
						value.vectorWidth = 0;
					}
					// A call cannot give these as an argument either (parseTemplateArgument, deduceTemplateArguments),
					// and the instance's key does not tell an enum from its underlying type.
					if (value.isPointer || value.arrayLength || value.resource != ResourceKind::None
						|| !value.enumName.empty() || value.isVolatile) {
						throw CompileError("the explicit specialization of \"" + written.name + "\" at line "
							+ std::to_string(written.line) + " gives the template parameter \"" + primary->parameters[j].name
							+ "\" a pointer, array, resource, enum or volatile type, which is not lowered");
					}
					if (known[j]) {
						fits = sameValueType(arguments[j].type, value);
					} else {
						arguments[j].type = value;
						known[j] = true;
					}
				}
			}
			for (size_t j = 0; j < primary->parameters.size() && fits; ++j) {
				if (known[j] || (!primary->parameters[j].isType && !arguments[j].unbound)) {
					continue;
				}
				if (!primary->parameters[j].hasDefault) {
					fits = false;
					break;
				}
				try {
					arguments[j] = templateDefault(*primary, j, arguments);
				} catch (const SubstitutionFailure&) {
					fits = false;
				}
			}
			if (!fits) {
				continue;
			}
			FunctionDecl instance;
			try {
				instance = instantiateTemplate(*primary, arguments, false);
			} catch (const SubstitutionFailure&) {
				continue;
			}
			if (typeName(instance.returnType) != typeName(signature.returnType)
				|| !std::equal(instance.parameters.begin(), instance.parameters.end(),
					signature.parameters.begin(), signature.parameters.end(), sameParameter)) {
				continue;
			}
			matches.emplace_back(primary, std::move(arguments));
		}
		const std::string where = "the explicit specialization of \"" + written.name + "\" at line "
			+ std::to_string(written.line);
		if (matches.empty()) {
			throw CompileError(where + " matches no function template of that name declared above it");
		}
		if (matches.size() > 1) {
			throw CompileError(where + " matches more than one function template of that name, and choosing "
				"among them by partial ordering is not lowered");
		}
		const std::string key = instanceKey(*matches[0].first, matches[0].second);
		if (!_specializationOf.emplace(key, &specialization).second) {
			throw CompileError(where + " specializes an instance that is already explicitly specialized");
		}
	}
}

// The explicit specialization that is the instance's body, or null for the primary template's.
// Apple refuses one declared after a use of the instance, so it never stands in silently.
const ExplicitSpecialization* Emitter::specializationFor(const TemplateInstance& instance) const {
	const auto found = _specializationOf.find(instanceKey(*instance.declaration, instance.arguments));
	if (found == _specializationOf.end()) {
		return nullptr;
	}
	if (found->second->declaration.order > instantiationOrder()) {
		throw CompileError("the explicit specialization of \"" + instance.declaration->name + "\" at line "
			+ std::to_string(found->second->declaration.line) + " is declared after a use of the instance it "
			"specializes");
	}
	return found->second;
}

// The instance for complete arguments, signature read and body not: one per argument list.
Emitter::TemplateInstance& Emitter::templateInstance(const FunctionTemplate& declaration,
	std::vector<TemplateArgument> arguments) {
	const std::string key = instanceKey(declaration, arguments);
	std::unique_ptr<TemplateInstance>& slot = _instances[key];
	if (slot) {
		return *slot;
	}
	FunctionDecl signature = instantiateTemplate(declaration, arguments, false);
	slot = std::make_unique<TemplateInstance>();
	slot->declaration = &declaration;
	slot->arguments = std::move(arguments);
	slot->decl = std::move(signature);
	slot->helper.definition = &slot->decl;
	slot->helper.firstOrder = declaration.order;
	_instanceOf[&slot->decl] = slot.get();
	return *slot;
}

// The instance is the function a call chose: its body is read now.
void Emitter::commitInstance(TemplateInstance& instance) {
	const ExplicitSpecialization* specialization = specializationFor(instance);
	if (instance.committed) {
		return;
	}
	instance.rootOrder = instantiationOrder();
	const FunctionTemplate& declaration = *instance.declaration;
	std::string spelled = declaration.name + "<";
	for (size_t i = 0; i < instance.arguments.size(); ++i) {
		spelled += (i ? ", " : "") + templateArgumentName(instance.arguments[i]);
	}
	spelled += ">";
	if (!declaration.hasBody && !specialization) {
		throw CompileError("the function template \"" + declaration.name + "\" is declared and never defined");
	}
	const auto caller = _instanceOf.find(_helper);
	const size_t depth = caller == _instanceOf.end() ? 1 : caller->second->depth + 1;
	if (depth > kMaxInstantiationDepth) {
		throw CompileError("template instantiation depth exceeds " + std::to_string(kMaxInstantiationDepth)
			+ " (instantiating " + spelled + "); Apple's limit is 1024");
	}
	instance.decl = specialization ? instantiateTemplate(specialization->declaration, {}, true)
		: instantiateTemplate(declaration, instance.arguments, true);
	instance.depth = depth;
	instance.committed = true;

	// The scans for file-scope samplers and imports ran before any instance existed.
	ScopeStack scopes(1);
	for (const Parameter& parameter: instance.decl.parameters) {
		scopes.back().insert(parameter.name);
	}
	std::set<std::string> names;
	collectUnshadowed(instance.decl.body.get(), scopes, names);
	for (size_t i = 0; i < instance.decl.globalsBefore && i < _unit.globals.size(); ++i) {
		if (_unit.globals[i].sampler && names.count(_unit.globals[i].name)) {
			throw CompileError("the instance " + spelled + " names the file-scope sampler \""
				+ _unit.globals[i].name + "\", which an instance does not reach; pass the sampler to it as an argument");
		}
	}
	if (_options.linkageObject) {
		std::vector<const Expression*> calls;
		collectCalls(*instance.decl.body, calls);
		for (const Expression* call: calls) {
			if (call->left->kind != ExpressionKind::Identifier) {
				continue;
			}
			for (const HelperFunction* called: helpersCalled(*call)) {
				if (!called->definition->body) {
					throw CompileError("the instance " + spelled + " calls \"" + called->definition->name
						+ "\", which this library only declares; importing from an instance of a template is "
						"not lowered");
				}
			}
		}
	}
}

// Whether an expression, written in a template, depends on its parameters: it names a parameter whose
// declared type does (not a local of that name). An explicit conversion names a concrete type here, so
// it is not, which only costs a refusal where the template wrote T(...); a call is when an argument is.
bool dependsOnParameter(const Expression& expression) {
	if (expression.kind == ExpressionKind::Identifier) {
		return expression.templateDependent;
	}
	if (expression.kind == ExpressionKind::Construct || expression.namesConcreteStruct) {
		return false;
	}
	const auto any = [](const ExpressionPtr& child) { return child && dependsOnParameter(*child); };
	return any(expression.left) || any(expression.right)
		|| std::any_of(expression.arguments.begin(), expression.arguments.end(), any)
		|| std::any_of(expression.elements.begin(), expression.elements.end(),
			[](const InitializerElement& element) { return dependsOnParameter(*element.value); });
}

bool Emitter::dependsOnTemplate(const Expression& call) {
	return std::any_of(call.arguments.begin(), call.arguments.end(),
		[](const ExpressionPtr& argument) { return dependsOnParameter(*argument); });
}

// Whether a call may be to a template that returns one of its own parameters.
bool Emitter::callsDependentTemplate(const Expression& call) {
	if (call.left->kind != ExpressionKind::Identifier) {
		return false;
	}
	for (const FunctionTemplate* declaration: templatesCalled(call)) {
		std::vector<TemplateArgument> arguments;
		const FunctionDecl* pattern = declaration->order > callerOrder() ? nullptr
			: templatePattern(*declaration, call.left->templateArguments, arguments);
		if (pattern && isTemplateMarker(pattern->returnType)) {
			return true;
		}
	}
	return false;
}

// Whether an argument brings no namespace to argument-dependent lookup ([basic.lookup.argdep]): it has no
// struct or enum type, nor has anything inside it. An operator's operands are looked into, so pointer
// arithmetic on a pointer to a struct is not taken for a scalar.
bool Emitter::bringsNoNamespace(const Expression& argument) {
	switch (argument.kind) {
		case ExpressionKind::Conditional:
		case ExpressionKind::Comma:
		case ExpressionKind::Assign:
			return false;
		case ExpressionKind::Binary:
			return argument.left && argument.right && bringsNoNamespace(*argument.left)
				&& bringsNoNamespace(*argument.right);
		case ExpressionKind::Unary:
			return argument.unaryOperator != UnaryOperator::AddressOf && argument.left
				&& bringsNoNamespace(*argument.left);
		case ExpressionKind::Call:
			if (callsDependentTemplate(argument)) {
				return deducedResultIsFundamental(argument);
			}
			break;
		default:
			break;
	}
	const std::optional<std::string> tag = enumOf(argument);
	return !adlStructOf(argument) && tag && tag->empty();
}

// Whether a call to a function template, whose result is known only once deduced, has a result that brings
// no namespace to argument-dependent lookup, whichever function it chooses. That holds when none of its own
// arguments brings one, so its candidates are only those ordinary lookup finds, and each of them returns a
// scalar or vector: a function or a template returning a type of no struct or enum, or a template returning
// a type parameter deduced from a parameter of exactly that type, a reference to it included. Deduction
// allows no conversion there ([temp.deduct.call]), so the argument's type is the parameter's.
bool Emitter::deducedResultIsFundamental(const Expression& call) {
	if (call.left->kind != ExpressionKind::Identifier) {
		return false;
	}
	for (const ExpressionPtr& argument: call.arguments) {
		if (!bringsNoNamespace(*argument)) {
			return false;
		}
	}
	const auto scalarOrVector = [](const Type& type) {
		return type.namedType.empty() && type.enumName.empty() && type.resource == ResourceKind::None;
	};
	for (const HelperFunction* helper: helpersCalled(call)) {
		if (returnTypeOf(*helper->definition).isAuto || !scalarOrVector(returnTypeOf(*helper->definition))) {
			return false;
		}
	}
	for (const FunctionTemplate* declaration: templatesCalled(call)) {
		std::vector<TemplateArgument> arguments;
		const FunctionDecl* pattern = declaration->order > callerOrder() ? nullptr
			: templatePattern(*declaration, call.left->templateArguments, arguments);
		if (!pattern) {
			continue;
		}
		if (pattern->returnType.isAuto) {
			return false;
		}
		if (!isTemplateMarker(pattern->returnType)) {
			if (!scalarOrVector(pattern->returnType)) {
				return false;
			}
			continue;
		}
		const std::string& marker = pattern->returnType.namedType;
		bool deduced = false;
		for (size_t i = 0; i < pattern->parameters.size() && i < call.arguments.size() && !deduced; ++i) {
			const Type& type = pattern->parameters[i].type;
			deduced = type.namedType == marker && !type.isPointer && !type.arrayLength;
		}
		if (!deduced) {
			return false;
		}
	}
	return true;
}

std::string Emitter::describeTemplate(const FunctionTemplate& declaration, const FunctionDecl& pattern) const {
	return "template " + declaration.name + "(" + parameterSignature(pattern) + ")";
}

} // namespace mslc
