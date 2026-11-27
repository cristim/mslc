#include "parser/internal.h"

namespace mslc {

Parameter Parser::parseParameter(const std::string& helperName, bool allowUnnamed) {
	Parameter param;
	const bool helper = !helperName.empty();

	param.type = parseType(true);

	if (param.type.isStatic || param.type.isConstexpr) {
		throw CompileError(std::string("a parameter cannot be ") + (param.type.isStatic ? "static" : "constexpr"));
	}

	// "constant BufferClearParams &params" and "constant BufferClearParams
	// *params" name the same buffer and lower to the same descriptor, so a
	// reference is consumed and nothing is recorded for it.
	if (at(TokenKind::Ampersand) && param.type.resource != ResourceKind::None) {
		throw CompileError("a reference to " + typeName(param.type) + " is not valid; a texture or "
			"sampler parameter is taken by value");
	}
	param.isReference = match(TokenKind::Ampersand);

	// A prototype and an unused parameter may leave the name out.
	const bool unnamed = (helper || allowUnnamed)
		&& (at(TokenKind::Comma) || at(TokenKind::RParen) || (allowUnnamed && at(TokenKind::LBracket)));
	if (!unnamed && kind() != TokenKind::Identifier) {
		throw CompileError("expected a parameter name, found " + std::string(tokenKindName(kind())));
	}

	if (!unnamed) {
		param.name = std::string(advance().text);
		declareLocal(param.name);
	}

	if (helper) {
		const std::string what = "parameter \"" + param.name + "\" of helper function \"" + helperName + "\" ";
		if (param.type.isStdArray && (param.type.isPointer || at(TokenKind::LBracket))) {
			throw CompileError(what + "is a pointer to or an array of array<T, N>, which is not lowered");
		}
		// "float p[3]", "float p[]" and "float m[3][3]" decay to a thread pointer to the
		// first element; the first length is dropped and the lengths inside it stay.
		if (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
			if (param.type.isPointer || param.isReference || param.type.resource != ResourceKind::None
				|| param.type.isAtomic) {
				throw CompileError(what + "is an array of pointers, references, atomics or resources, "
					"which a helper function does not take yet");
			}
			parseArrayDimensions(param.type);
			param.type.arrayLength.reset();
			param.type.isPointer = true;
			if (param.type.addressSpace == AddressSpace::None) {
				param.type.addressSpace = AddressSpace::Thread;
			}
		}
		// A pointer to the caller's local array, thread or threadgroup; the emitter checks the argument.
		const bool threadPointer = param.type.isPointer && !param.isReference
			&& (param.type.addressSpace == AddressSpace::Thread || param.type.addressSpace == AddressSpace::Threadgroup)
			&& param.type.resource == ResourceKind::None && !param.type.isAtomic;
		// A pointer to a buffer in device or constant memory; the emitter passes the
		// whole buffer the argument names.
		const bool bufferPointer = param.type.isPointer && !param.isReference
			&& param.type.resource == ResourceKind::None && !param.type.isAtomic
			&& (param.type.addressSpace == AddressSpace::Device
				|| param.type.addressSpace == AddressSpace::Constant);
		if (param.type.isPointer && !threadPointer && !bufferPointer) {
			throw CompileError(what + "is a pointer outside the thread, threadgroup, device and constant "
				"address spaces or to an atomic or a resource, which a helper function does not take yet; it "
				"takes scalar, vector, matrix and struct values, a thread or threadgroup pointer to a local "
				"array, and a device or constant pointer to a buffer");
		}
		if (param.type.arrayLength && !param.type.isStdArray) {
			throw CompileError(what + "is an array, which a helper function does not take yet");
		}
		// A reference to a thread variable may spell its space out; any other
		// parameter has none to name.
		// A reference into constant memory cannot change what it names, so it is const whether or not
		// the source says so; the emitter passes it by value and checks the argument is in that space.
		const bool constantReference = param.isReference && param.type.addressSpace == AddressSpace::Constant
			&& !param.type.isAtomic;
		if (constantReference) {
			param.type.isConst = true;
		}
		if (param.type.addressSpace != AddressSpace::None && !constantReference && !bufferPointer
			&& !((param.isReference || threadPointer) && param.type.addressSpace == AddressSpace::Thread)
			&& !threadPointer) {
			throw CompileError(what + "is in the " + addressSpaceName(param.type.addressSpace)
				+ " address space, which a helper function does not take yet");
		}
		if (at(TokenKind::LBracket)) {
			checkHelperParameterAttributes(param, what);
		}
		recordObjectType(param.name, param.type);
		return param;
	}

	if (param.type.isStdArray) {
		throw CompileError("parameter \"" + param.name + "\" is an array<T, N> of an entry point, which is not "
			"lowered; put the array in a struct, which a buffer holds");
	}

	if (param.type.resource != ResourceKind::None
		&& (param.type.isPointer || param.type.arrayLength
			|| (param.type.addressSpace != AddressSpace::None && param.type.addressSpace != AddressSpace::Thread))) {
		throw CompileError("parameter \"" + param.name + "\" is a pointer to or an array of "
			+ typeName(param.type) + " or has an address space other than thread, which is not lowered "
			"yet; mslc takes a texture or sampler by value");
	}

	// "float values[4]" is an array suffix, "[[buffer(0)]]" is an attribute
	// list, and both start with a bracket, so the second one has to be
	// checked before the first is consumed.
	const bool arraySuffix = at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket;
	if (arraySuffix) {
		advance();
		expect(TokenKind::RBracket, "to close an array parameter");
	}

	if (at(TokenKind::LBracket)) {
		param.attributes = parseParameterAttributes();
	}

	if (!arraySuffix) {
		recordObjectType(param.name, param.type);
	}
	return param;
}

// QUIRK: a parameter of a function that is not an entry point may carry the attributes an entry point's
// parameter takes ([[buffer(n)]], [[texture(n)]], [[stage_in]], [[user(...)]], a builtin, ...), and they have no
// effect: the argument is what the call passes, and indices may repeat. Some must still suit the type, else it
// is an error: [[buffer(n)]] a pointer or reference to device or constant memory, [[threadgroup(n)]] one to
// threadgroup memory, [[texture(n)]] a texture, [[sampler(n)]] a sampler, [[position]] a float4, and the
// thread and threadgroup position, index and size builtins a uint or ushort scalar or vector. In a function
// template the check is made on the instance, as a substitution failure. [[flat]] and the other interpolation
// attributes apply to struct members only. The other attributes are accepted and dropped without a check of the
// type. The MSL specification does not say. Evidence: black-box observation of Apple's compiler (tier 2; metal
// 32023.883, macOS 26), which accepts the well-typed forms and reports "type ... is not valid for attribute",
// "invalid address space qualification for buffer pointee type" or a substitution failure for the others.
// Test: probes helper_parameter_attribute_*.
void Parser::checkHelperParameterAttributes(const Parameter& param, const std::string& what) {
	const size_t start = _position;
	const Type& type = param.type;
	// A type still to be deduced is checked on the instance.
	const bool dependent = isDeductionMarker(type) || (type.isTexture() && !type.namedType.empty());
	const bool place = type.isPointer || param.isReference;
	const bool resource = type.resource != ResourceKind::None;
	const bool integerBuiltinType = !type.isPointer && !type.arrayLength && !type.isMatrix() && type.namedType.empty()
		&& (type.scalar == ScalarKind::UInt || type.scalar == ScalarKind::UShort) && type.vectorWidth <= 3;
	std::string wrong;
	std::set<std::string> seen;
	parseAttributeList(seen, [&](const std::string& name, std::optional<uint32_t>, const std::string&) {
		if (interpolationFromName(name)) {
			throw CompileError(what + "has the attribute [[" + name + "]], which applies to a struct member only");
		}
		if (dependent || !wrong.empty()) {
			return;
		}
		const bool fits = name == "buffer" ? place && !resource
				&& (type.addressSpace == AddressSpace::Device || type.addressSpace == AddressSpace::Constant)
			: name == "threadgroup" ? place && !resource && type.addressSpace == AddressSpace::Threadgroup
			: name == "texture" ? type.isTexture() && !type.isPointer && !type.arrayLength
			: name == "sampler" ? type.resource == ResourceKind::Sampler && !type.isPointer && !type.arrayLength
			: name == "position" ? !place && !resource && type.namedType.empty() && type.scalar == ScalarKind::Float
				&& type.vectorWidth == 4 && !type.isMatrix() && !type.arrayLength
			: builtinFromName(name) && name != "front_facing" && name != "vertex_id" && name != "instance_id"
				&& name != "frag_coord" ? integerBuiltinType
			: true;
		if (!fits) {
			wrong = name;
		}
	});
	if (!wrong.empty()) {
		_position = start;
		const std::string message = what + "has an attribute, [[" + wrong + "]], that its type " + typeName(type)
			+ " is not valid for";
		if (_inInstance) {
			throw SubstitutionFailure(message);
		}
		throw CompileError(message);
	}
}

ParameterAttributes Parser::parseParameterAttributes() {
	ParameterAttributes attributes;
	std::set<std::string> seen;

	const auto visit = [&](const std::string& name, std::optional<uint32_t> argument, const std::string& identifier) {
		if (name == "function_constant") {
			if (identifier.empty()) {
				throw CompileError("[[function_constant]] on a parameter needs the name of a program-scope constant");
			}
			attributes.functionConstant = identifier;
			return;
		}
		if (argument && (name == "buffer" || name == "texture" || name == "sampler" || name == "threadgroup")) {
			checkAttributeIndex(name, *argument);
		}

		if (name == "buffer") {
			if (!argument) {
				throw CompileError("[[buffer]] needs an index");
			}
			attributes.bufferIndex = argument;
		} else if (name == "threadgroup") {
			if (!argument) {
				throw CompileError("[[threadgroup]] needs an index");
			}
			attributes.threadgroupIndex = argument;
		} else if (name == "texture") {
			if (!argument) {
				throw CompileError("[[texture]] needs an index");
			}
			attributes.textureIndex = argument;
		} else if (name == "sampler") {
			if (!argument) {
				throw CompileError("[[sampler]] needs an index");
			}
			attributes.samplerIndex = argument;
		} else if (name == "depth") {
			throw CompileError("[[depth]] is not valid on an input parameter");
		} else if (name == "stencil") {
			throw CompileError("[[stencil]] is not valid on an input parameter");
		} else if (name == "sample_mask") {
			throw CompileError("[[sample_mask]] on an input parameter is not lowered yet");
		} else if (name == "invariant") {
			throw CompileError("[[invariant]] is only valid on a vertex output's [[position]] field");
		} else if (name == "stage_in") {
			attributes.stageIn = true;
		} else if (name == "color") {
			if (!argument) {
				throw CompileError("[[color]] needs an index");
			}
			attributes.colorIndex = argument;
		} else if (auto builtin = builtinFromName(name)) {
			attributes.builtin = builtin;
		} else if (isMSLBuiltinName(name)) {
			throw CompileError("builtin attribute \"" + name + "\" is recognised but not "
				"supported yet; mslc reports it rather than ignoring it");
		} else {
			throw CompileError("unsupported attribute \"" + name + "\"");
		}
	};
	do {
		parseAttributeList(seen, visit);
	} while (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket);

	if (!attributes.functionConstant.empty() && attributes.stageIn) {
		throw CompileError("[[function_constant]] on a [[stage_in]] parameter is not lowered yet");
	}

	return attributes;
}

FieldAttributes Parser::parseFieldAttributes() {
	FieldAttributes attributes;
	std::set<std::string> seen;

	const auto visit = [&](const std::string& name, std::optional<uint32_t> argument,
		const std::string& identifier) {
		if (name == "depth") {
			if (attributes.depthMode) {
				throw CompileError("a struct field has more than one [[depth]] attribute");
			}
			if (identifier == "any") {
				attributes.depthMode = DepthMode::Any;
			} else if (identifier == "less") {
				attributes.depthMode = DepthMode::Less;
			} else if (identifier == "greater") {
				attributes.depthMode = DepthMode::Greater;
			} else {
				throw CompileError("[[depth]] needs one of any, less or greater");
			}
		} else if (name == "sample_mask") {
			if (attributes.sampleMask) {
				throw CompileError("a struct field has more than one [[sample_mask]] attribute");
			}
			attributes.sampleMask = true;
		} else if (name == "point_size") {
			if (attributes.pointSize) {
				throw CompileError("a struct field has more than one [[point_size]] attribute");
			}
			attributes.pointSize = true;
		} else if (name == "stencil") {
			if (attributes.stencil) {
				throw CompileError("a struct field has more than one [[stencil]] attribute");
			}
			attributes.stencil = true;
		} else if (name == "user") {
			if (identifier.empty()) {
				throw CompileError("[[user]] needs a name: [[user(name)]]");
			}
			if (attributes.userName) {
				throw CompileError("a struct field has more than one [[user]] attribute");
			}
			attributes.userName = identifier;
		} else if (name == "position") {
			attributes.position = true;
		} else if (name == "invariant") {
			attributes.invariant = true;
		} else if (name == "attribute") {
			if (!argument) {
				throw CompileError("[[attribute]] needs an index");
			}
			checkAttributeIndex(name, *argument);
			attributes.attributeIndex = argument;
		} else if (name == "color") {
			if (!argument) {
				throw CompileError("[[color]] needs an index");
			}
			attributes.colorIndex = argument;
		} else if (name == "function_constant") {
			if (identifier.empty()) {
				throw CompileError("[[function_constant]] on a struct field needs the name of a program-scope constant");
			}
			attributes.functionConstant = identifier;
		} else if (const auto interpolation = interpolationFromName(name)) {
			if (attributes.interpolation != Interpolation::None) {
				throw CompileError("a struct field has more than one interpolation attribute; "
					"\"" + name + "\" follows another");
			}
			attributes.interpolation = *interpolation;
		} else if (builtinFromName(name)) {
			throw CompileError("builtin attribute \"" + name + "\" is not valid on a struct "
				"field; only [[position]], [[invariant]], [[attribute(n)]], [[color(n)]], [[depth(mode)]], [[sample_mask]], [[point_size]], [[user(name)]] and the "
				"interpolation attributes are");
		} else {
			throw CompileError("unsupported attribute \"" + name + "\" on a struct field");
		}
	};
	do {
		parseAttributeList(seen, visit);
	} while (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket);

	// A gated stage input or output other than a colour would change the interface between stages, which is
	// fixed when a pipeline is made here.
	if (!attributes.functionConstant.empty() && !attributes.colorIndex) {
		throw CompileError("[[function_constant]] on a struct field is lowered on a [[color(n)]] field only");
	}

	return attributes;
}

void Parser::parseAttributeList(std::set<std::string>& seen, const std::function<void(const std::string&,
	std::optional<uint32_t>, const std::string&)>& visit) {
	expect(TokenKind::LBracket, "to open an attribute list");
	expect(TokenKind::LBracket, "to open an attribute");

	while (!at(TokenKind::EndOfFile)) {
		if (match(TokenKind::RBracket)) {
			break;
		}
		match(TokenKind::RBracket);

		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected an attribute name, found " + std::string(tokenKindName(kind())));
		}

		const std::string name(advance().text);

		// [[user]], [[depth]], [[sample_mask]], [[point_size]] and the interpolation
		// attributes say more than once in their own words, where the field is parsed.
		if (!seen.insert(name).second && name != "user" && name != "depth" && name != "sample_mask"
			&& name != "point_size" && name != "stencil" && !interpolationFromName(name)) {
			throw CompileError("attribute \"" + name + "\" cannot appear more than once on a declaration");
		}

		std::optional<uint32_t> argument;
		std::string identifier;
		if (at(TokenKind::LParen)) {
			if (name == "sample_mask") {
				throw CompileError("[[sample_mask]] takes no argument");
			}
			if (name == "point_size") {
				throw CompileError("[[point_size]] takes no argument");
			}
			if (name == "invariant") {
				throw CompileError("[[invariant]] takes no argument");
			}
			if (name == "stencil") {
				throw CompileError("[[stencil]] takes no argument");
			}
			advance();
			if (name == "user" || name == "depth") {
				if (kind() != TokenKind::Identifier) {
					throw CompileError("attribute \"" + name + "\" needs an identifier, found "
						+ std::string(tokenKindName(kind())));
				}
				identifier = std::string(advance().text);
			} else if (name == "function_constant") {
				parseFunctionConstantArgument(argument, identifier);
			} else {
				argument = static_cast<uint32_t>(parseConstantIndex("attribute \"" + name + "\"", true));
			}
			expect(TokenKind::RParen, "to close an attribute argument");
		}

		visit(name, argument, identifier);

		match(TokenKind::Comma);
	}

	expect(TokenKind::RBracket, "to close an attribute list");
}

// The argument of [[function_constant(...)]]. On a file-scope constant it is the constant's index, a constant
// integer expression; anywhere else it names the program-scope constant that gates the declaration. Which one
// is wanted is up to the caller, so both are worked out: index gets the value of an integer constant
// expression and gate the name of a file-scope constant.
void Parser::parseFunctionConstantArgument(std::optional<uint32_t>& index, std::string& gate) {
	const size_t start = _position;
	const ExpressionPtr expression = parseAssignment();
	// The file-scope names, which the parser of a template's instance is given as well (restoreNames). A
	// parameter or local of that name hides the constant, and is not a program-scope constant.
	bool hidden = false;
	for (const auto& block: _blocks) {
		hidden = hidden || block.names.count(expression->name);
	}
	if (expression->kind == ExpressionKind::Identifier && !hidden) {
		const auto declared = _declared.find(expression->name);
		if (declared != _declared.end() && declared->second == "variable") {
			gate = expression->name;
		}
	}
	const size_t end = _position;
	_position = start;
	try {
		index = static_cast<uint32_t>(parseConstantIndex("attribute \"function_constant\"", true));
	} catch (const CompileError& error) {
		if (gate.empty()) {
			throw CompileError(std::string(error.what()) + "; on a declaration other than a file-scope constant, it "
				"names a program-scope constant");
		}
		index.reset();
	}
	_position = end;
}

FunctionDecl Parser::parseFunctionDeclaration(Stage stage) {
	FunctionDecl decl;
	decl.stage = stage;
	decl.line = line();
	decl.globalsBefore = _unit.globals.size();
	decl.order = _functionOrder++;

	decl.returnType = parseType();

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a function name, found " + std::string(tokenKindName(kind())));
	}

	decl.name = std::string(advance().text);
	if (std::find(_namespacePath.begin(), _namespacePath.end(), kAnonymousNamespace) != _namespacePath.end()) {
		throw CompileError(std::string(stage == Stage::Kernel ? "kernel" : stage == Stage::Vertex ? "vertex" : "fragment") + " function cannot be declared in anonymous namespace");
	}
	decl.name = qualify(decl.name);
	declareName(decl.name, "function");
	const LocalScope parameters(*this);

	expect(TokenKind::LParen, "to open a parameter list");

	while (!at(TokenKind::RParen) && !at(TokenKind::EndOfFile)) {
		decl.parameters.push_back(parseParameter());
		if (!match(TokenKind::Comma)) {
			break;
		}
	}

	expect(TokenKind::RParen, "to close a parameter list");
	rejectUsingConflict(decl.name, "function", &decl);
	checkUniqueResourceSlots(decl);

	// parseCompoundStatement consumes the opening brace itself, so consuming it
	// here as well would make it look for a second one.
	decl.body = parseCompoundStatement(true);

	return decl;
}

void Parser::parseEntryParameterList(std::vector<Parameter>& out, const std::string& quoted, bool allowUnnamed) {
	if (atKeyword("void") && lookahead().kind == TokenKind::RParen) { advance(); }
	while (!at(TokenKind::RParen) && !at(TokenKind::EndOfFile)) {
		out.push_back(parseParameter(std::string(), allowUnnamed));
		if (at(TokenKind::Assign)) {
			throw CompileError("a default argument is not lowered yet (parameter \"" + out.back().name + "\" of " + quoted + ")");
		}
		if (!match(TokenKind::Comma)) { break; }
	}
	expect(TokenKind::RParen, "to close a parameter list");
}

void Parser::checkUniqueResourceSlots(const FunctionDecl& decl) const {
	// Two parameters of one kind cannot take the same slot, unless every one of them but one is gated by
	// [[function_constant]]. Apple's compiler accepts such a slot, as when a function takes its source as a
	// texture2d or a texture2d_array at texture(0) and a function constant picks one (metal 32023.883, macOS 26);
	// each such parameter has a binding of its own here.
	std::set<std::pair<std::string, uint32_t>> slots;
	for (const Parameter& parameter: decl.parameters) {
		if (!parameter.attributes.functionConstant.empty()) {
			continue;
		}
		const std::pair<const char*, std::optional<uint32_t>> taken[] = {
			{ "buffer", parameter.attributes.bufferIndex },
			{ "texture", parameter.attributes.textureIndex },
			{ "sampler", parameter.attributes.samplerIndex },
		};
		for (const auto& [kindName, index]: taken) {
			if (index && !slots.emplace(kindName, *index).second) {
				throw CompileError("the " + std::string(kindName) + " index " + std::to_string(*index)
					+ " is used by more than one parameter of \"" + decl.name + "\"");
			}
		}
	}
}

namespace {

bool fitsNonTypeTemplateConstant(ScalarKind kind, int64_t value) {
	if (kind == ScalarKind::Bool) { return value == 0 || value == 1; }
	int64_t low = 0, high = 0;
	return integerRange(kind, low, high) && value >= low && value <= high;
}

bool sameFullType(Type lhs, Type rhs, bool checkTopConst) {
	if (!checkTopConst && !lhs.isPointer) { lhs.isConst = false; }
	if (!checkTopConst && !rhs.isPointer) { rhs.isConst = false; }
	return sameType(lhs, rhs) && lhs.isPointer == rhs.isPointer && lhs.addressSpace == rhs.addressSpace
		&& lhs.resource == rhs.resource && lhs.textureAccess == rhs.textureAccess
		&& lhs.isDepth == rhs.isDepth && lhs.isMultisampled == rhs.isMultisampled;
}

auto parameterAttributeKey(const ParameterAttributes& a) {
	return std::tie(a.bufferIndex, a.threadgroupIndex, a.textureIndex, a.samplerIndex, a.builtin, a.stageIn, a.colorIndex, a.functionConstant);
}

}

bool Parser::deduceEntryArguments(const FunctionTemplate& tmpl, const FunctionDecl& pattern,
	size_t given, const std::vector<Parameter>& gotParams, std::vector<TemplateArgument>& arguments) const {
	if (pattern.parameters.size() != gotParams.size()) { return false; }
	std::vector<bool> known(arguments.size(), false);
	std::fill_n(known.begin(), given, true);
	const auto matchType = [&](const auto& self, const Type& patType, const Type& gotType, bool ptrOrRef, bool inInst) -> bool {
		if (pattern.dependentInstances.count(patType.namedType)) {
			if ((ptrOrRef && patType.isConst != gotType.isConst) || (inInst && (gotType.isPointer || gotType.arrayLength))) { return false; }
			const DependentInstance& expected = pattern.dependentInstances.at(patType.namedType);
			const StructDecl* made = gotType.namedType.empty() ? nullptr : _unit.findStruct(gotType.namedType);
			if (!made || made->classTemplate != expected.classTemplate || made->classArguments.size() != expected.arguments.size()) { return false; }
			for (size_t k = 0; k < expected.arguments.size(); ++k) {
				const TemplateArgument& want = expected.arguments[k], &got = made->classArguments[k];
				if (want.isType != got.isType || (!want.isType && want.value != got.value)
					|| (want.isType && !self(self, want.type, got.type, true, true))) { return false; }
			}
			return true;
		}
		size_t idx = tmpl.parameters.size();
		for (size_t j = given; j < tmpl.parameters.size(); ++j) {
			if (tmpl.parameters[j].isType && !tmpl.parameters[j].name.empty() && patType.namedType == "<" + tmpl.parameters[j].name + ">") { idx = j; break; }
		}
		if (idx == tmpl.parameters.size()) { return !inInst || sameFullType(patType, gotType, true); }
		if (patType.isConst && !gotType.isConst && ptrOrRef) { return false; }
		if (gotType.resource != ResourceKind::None || !gotType.enumName.empty() || gotType.isVolatile || gotType.isAtomic) { return false; }
		Type bound = gotType;
		bound.isPointer = bound.isStdArray = false;
		bound.addressSpace = AddressSpace::None;
		bound.arrayLength.reset();
		bound.innerArrayLengths.clear();
		if (patType.isConst || !ptrOrRef) { bound.isConst = false; }
		if (known[idx]) { return sameFullType(arguments[idx].type, bound, true); }
		known[idx] = true;
		arguments[idx] = TemplateArgument { true, bound, 0, false };
		return true;
	};
	for (size_t i = 0; i < pattern.parameters.size(); ++i) {
		const Parameter& pat = pattern.parameters[i], &got = gotParams[i];
		const bool patPtr = pat.type.isPointer || pat.type.arrayLength.has_value(), gotPtr = got.type.isPointer || got.type.arrayLength.has_value();
		if (pat.isReference != got.isReference || patPtr != gotPtr
			|| ((patPtr || pat.isReference) && pat.type.addressSpace != got.type.addressSpace)
			|| !matchType(matchType, pat.type, got.type, patPtr || pat.isReference, false)) { return false; }
	}
	for (size_t j = given; j < tmpl.parameters.size(); ++j) {
		if (tmpl.parameters[j].isType && !known[j]) {
			if (!tmpl.parameters[j].hasDefault) { return false; }
			try { arguments[j] = templateDefault(tmpl, j, arguments); } catch (const SubstitutionFailure&) { return false; }
		} else if (!tmpl.parameters[j].isType && arguments[j].unbound) { return false; }
	}
	return true;
}

void Parser::resolveEntryInstantiations() {
	for (const PendingEntryInstantiation& pending: _pendingEntryInstantiations) {
		for (size_t i = 0; i < _unit.functions.size(); ++i) {
			if (i != pending.slotIndex && _unit.functions[i].name == pending.hostName) {
				throw CompileError("two entry points are named \"" + pending.hostName + "\"; entry points are identified by name, so the names must differ");
			}
		}
		FunctionDecl pendingSig { pending.stage, pending.returnType, {}, pending.parameters, nullptr, pending.line };
		const FunctionTemplate* chosen = nullptr;
		std::vector<TemplateArgument> chosenArgs;
		bool sawProtoMatch = false, sawStageMismatch = false;
		for (const FunctionTemplate& tmpl: _unit.entryTemplates) {
			if (!pending.candidateNames.count(tmpl.name)) { continue; }
			if (tmpl.stage != pending.stage) { sawStageMismatch = true; continue; }
			if (pending.explicitArgs.size() > tmpl.parameters.size()) { continue; }
			std::vector<TemplateArgument> arguments(tmpl.parameters.size());
			bool argsFit = true;
			for (size_t i = 0; i < tmpl.parameters.size() && argsFit; ++i) {
				const TemplateParameter& p = tmpl.parameters[i];
				if (i < pending.explicitArgs.size()) {
					arguments[i] = pending.explicitArgs[i];
					argsFit = arguments[i].isType == p.isType && (p.isType || fitsNonTypeTemplateConstant(p.kind, arguments[i].value));
				} else if (p.isType) { arguments[i].type.namedType = "<" + p.name + ">"; }
				else if (p.hasDefault) {
					try { arguments[i] = templateDefault(tmpl, i, arguments); argsFit = fitsNonTypeTemplateConstant(p.kind, arguments[i].value); }
					catch (const SubstitutionFailure&) { argsFit = false; }
				} else { arguments[i] = { false, {}, 0, true }; }
			}
			if (!argsFit) { continue; }
			try {
				FunctionDecl pattern = instantiateTemplate(tmpl, arguments, false);
				if (!deduceEntryArguments(tmpl, pattern, pending.explicitArgs.size(), pending.parameters, arguments)) { continue; }
				const FunctionDecl finalSig = pending.explicitArgs.size() == tmpl.parameters.size()
					? std::move(pattern) : instantiateTemplate(tmpl, arguments, false);
				if (!sameFullType(finalSig.returnType, pendingSig.returnType, false) || !sameParameterTypes(finalSig, pendingSig)) { continue; }
			} catch (const SubstitutionFailure&) { continue; }
			if (!tmpl.hasBody) { sawProtoMatch = true; continue; }
			if (chosen) { throw CompileError("explicit instantiation of \"" + pending.spelled + "\" at line " + std::to_string(pending.line) + " is ambiguous"); }
			chosen = &tmpl;
			chosenArgs = std::move(arguments);
		}
		if (!chosen) {
			const std::string at = " at line " + std::to_string(pending.line);
			throw CompileError(sawProtoMatch
				? "the entry-point template \"" + pending.spelled + "\" (instantiated" + at + ") is declared and never defined"
				: "explicit instantiation of \"" + pending.spelled + "\"" + at
					+ (sawStageMismatch ? " does not match the entry-point template's stage" : " does not match any entry-point template"));
		}
		FunctionDecl instantiated = instantiateTemplate(*chosen, chosenArgs, true);
		for (size_t i = 0; i < pending.parameters.size(); ++i) {
			const auto givenKey = parameterAttributeKey(pending.parameters[i].attributes);
			if (givenKey != parameterAttributeKey(ParameterAttributes{})
				&& givenKey != parameterAttributeKey(instantiated.parameters[i].attributes)) {
				throw CompileError("parameter attributes in explicit instantiation of \"" + pending.spelled
					+ "\" at line " + std::to_string(pending.line) + " do not match the entry-point template");
			}
		}
		instantiated.name = pending.hostName;
		_unit.functions[pending.slotIndex] = std::move(instantiated);
	}
}

Parser::NestingScope::NestingScope(Parser& parser): _parser(parser) {
	if (++_parser._nesting > kMaxNestingDepth) {
		throw CompileError("statements and expressions are nested more than "
			+ std::to_string(kMaxNestingDepth) + " deep");
	}
}

void Parser::measure(Expression& expression) const {
	uint32_t tallest = 0;
	const auto consider = [&tallest](const ExpressionPtr& child) {
		if (child) {
			tallest = std::max(tallest, child->height);
		}
	};
	consider(expression.left);
	consider(expression.right);
	for (const ExpressionPtr& argument: expression.arguments) {
		consider(argument);
	}
	for (const InitializerElement& element: expression.elements) {
		consider(element.value);
	}

	expression.height = tallest + 1;
	if (expression.height > kMaxExpressionHeight) {
		throw CompileError("an expression is more than " + std::to_string(kMaxExpressionHeight)
			+ " operators deep");
	}
}

} // namespace mslc
