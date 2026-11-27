#include "parser/internal.h"

namespace mslc {

// "__attribute__((...))" ahead of a helper's return type, as METAL_FUNC expands to. The ones that
// only steer inlining or diagnostics change no value and are dropped; any other is refused, since
// ignoring one that matters (noreturn, aligned) would compile a different program.
void Parser::skipFunctionAttribute() {
	advance();
	const size_t first = _position;
	skipBalanced(TokenKind::LParen, TokenKind::RParen);
	int depth = 0;
	for (size_t i = first; i < _position; ++i) {
		if (_tokens[i].kind == TokenKind::LParen) {
			++depth;
		} else if (_tokens[i].kind == TokenKind::RParen) {
			--depth;
		} else if (depth == 2 && (_tokens[i - 1].kind == TokenKind::LParen || _tokens[i - 1].kind == TokenKind::Comma)) {
			const std::string name(_tokens[i].text);
			if (name != "always_inline" && name != "__always_inline__" && name != "noinline" && name != "__noinline__"
				&& name != "unused" && name != "__unused__" && name != "used" && name != "__used__") {
				throw CompileError("the function attribute \"" + name + "\" is not lowered; only always_inline, "
					"noinline, unused and used are accepted and dropped");
			}
		}
	}
}

bool Parser::skipStandardFunctionAttribute(bool forTemplateInstance) {
	const size_t start = _position;
	expect(TokenKind::LBracket, "to open an attribute list");
	expect(TokenKind::LBracket, "to open an attribute");

	// On a template instance, an unknown [[...]] restores _position so parseInstance reports its own diagnostic.
	if (!at(TokenKind::RBracket)) {
		if (atKeyword("using")) {
			if (forTemplateInstance) {
				_position = start;
				return false;
			}
			advance();
			const std::string scope = kind() == TokenKind::Identifier ? std::string(advance().text) : std::string();
			throw CompileError("an attribute namespace ("
				+ (scope.empty() ? std::string("using") : "using " + scope + ":")
				+ ") in [[...]] is not lowered; only [[maybe_unused]] is accepted and dropped");
		}
		for (;;) {
			if (kind() != TokenKind::Identifier || atKeyword("using")) {
				throw CompileError("expected an attribute name in [[...]]");
			}
			std::string name(advance().text);
			if (at(TokenKind::ColonColon)) {
				if (forTemplateInstance) {
					_position = start;
					return false;
				}
				while (match(TokenKind::ColonColon)) {
					if (kind() != TokenKind::Identifier) {
						throw CompileError("expected an attribute name in [[...]]");
					}
					name += "::" + std::string(advance().text);
				}
				throw CompileError("the function attribute \"" + name + "\" is not lowered; only always_inline, "
					"noinline, unused, used and [[maybe_unused]] are accepted and dropped");
			}
			if (name != "maybe_unused") {
				if (forTemplateInstance) {
					_position = start;
					return false;
				}
				throw CompileError("the function attribute \"" + name + "\" is not lowered; only always_inline, "
					"noinline, unused, used and [[maybe_unused]] are accepted and dropped");
			}
			if (at(TokenKind::LParen)) {
				throw CompileError("[[maybe_unused]] takes no argument");
			}
			if (!match(TokenKind::Comma)) {
				break;
			}
		}
	}

	expect(TokenKind::RBracket, "to close an attribute");
	expect(TokenKind::RBracket, "to close an attribute list");
	return true;
}

bool Parser::tryParseHelperFunction() {
	if (kind() != TokenKind::Identifier && !at(TokenKind::ColonColon)
		&& !(at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket)) {
		return false;
	}

	const size_t start = _position;
	FunctionDecl decl;
	decl.line = line();
	bool sawStandardAttribute = false;

	// inline, static, constexpr and attributes in any order, ahead of the return type. A refused
	// attribute throws here, outside the try below, so that it is not read as "not a function".
	for (;;) {
		if (atKeyword("__attribute__") && lookahead().kind == TokenKind::LParen) {
			skipFunctionAttribute();
		} else if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
			skipStandardFunctionAttribute(false);
			sawStandardAttribute = true;
		} else if (atKeyword("inline") || atKeyword("static") || atKeyword("constexpr")) {
			decl.isLocal = true;
			advance();
		} else {
			break;
		}
	}

	bool isHelper = false;
	try {
		if (!(sawStandardAttribute && (atKeyword("kernel") || atKeyword("vertex") || atKeyword("fragment")))) {
			decl.returnType = parseType();
			if (kind() == TokenKind::Identifier) {
				decl.name = std::string(advance().text);
				isHelper = at(TokenKind::LParen);
			}
		}
	} catch (const ClassTemplateError&) {
		throw;
	} catch (const CompileError&) {
	}
	if (!isHelper) {
		if (sawStandardAttribute) {
			throw CompileError("[[maybe_unused]] at file or namespace scope is only lowered ahead of a helper function");
		}
		_position = start;
		return false;
	}

	advance();
	decl.order = _functionOrder++;
	decl.globalsBefore = _unit.globals.size();

	const std::string quoted = "helper function \"" + decl.name + "\"";
	rejectUnloweredReturn(decl.returnType, quoted, true);

	Type builtin;
	if (isTypeName(decl.name, builtin) || isSimdVectorName(decl.name, builtin)) {
		throw CompileError(quoted + " has the name of a builtin type");
	}
	decl.name = qualify(decl.name);
	declareName(decl.name, "function");
	_helperNames.insert(decl.name);
	const LocalScope parameters(*this);
	parseHelperParameters(decl, quoted);
	rejectUsingConflict(decl.name, "function", &decl);

	// [namespace.udecl]: a using declaration names the overloads declared above it; one declared
	// later is not found through it. That is not modelled, so such an overload is refused.
	const bool redeclaration = std::any_of(_unit.helpers.begin(), _unit.helpers.end(), [&](const FunctionDecl& other) {
		return other.name == decl.name && sameParameterTypes(other, decl);
	});
	if (!redeclaration) {
		for (const auto& scope: _scopes) {
			for (const auto& brought: scope.second.declarations) {
				if (brought.second.count(decl.name)) {
					throw CompileError(quoted + " is a new overload declared after a using declaration of its "
						"name; Apple does not find it through that declaration, which mslc does not model");
				}
			}
		}
	}

	if (at(TokenKind::LBracket)) {
		throw CompileError("attributes on " + quoted + " are not lowered yet");
	}

	if (match(TokenKind::Semicolon)) {
		_unit.helpers.push_back(std::move(decl));
		return true;
	}

	decl.body = parseCompoundStatement(true);
	_unit.helpers.push_back(std::move(decl));
	return true;
}

void Parser::parseGlobalDeclaration() {
	VariableDeclaration declaration;

	Type base;
	declaration.type = parseType(true, false, &base);

	// A sampler is the one resource lowered at file scope, as an embedded sampler.
	// Apple takes it with or without constexpr, in the constant address space or
	// none, and "thread" is accepted too, as for a local.
	if (declaration.type.resource != ResourceKind::None) {
		if (declaration.type.resource != ResourceKind::Sampler) {
			throw CompileError("a file-scope " + typeName(declaration.type) + " is not lowered; a texture is "
				"an entry point parameter");
		}
		if (declaration.type.addressSpace == AddressSpace::Device
			|| declaration.type.addressSpace == AddressSpace::Threadgroup) {
			throw CompileError("only a constant can be declared at file scope, and a "
				+ std::string(addressSpaceName(declaration.type.addressSpace)) + " sampler is not one");
		}
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
		}
		declaration.name = qualify(std::string(advance().text));
		declareName(declaration.name, "variable");
		parseSamplerLocal(declaration);
		if (!declaration.sampler) {
			throw CompileError("the file-scope sampler \"" + declaration.name + "\" is initialised from "
				"\"sampler(options)\" or a brace list only; a copy of another sampler is not lowered at file scope");
		}
		_unit.globals.push_back(std::move(declaration));
		return;
	}

	if (declaration.type.addressSpace != AddressSpace::Constant) {
		throw CompileError("only a constant can be declared at file scope, and a "
			+ std::string(addressSpaceName(declaration.type.addressSpace))
			+ (declaration.type.isPointer ? " pointer" : " value") + " is not one");
	}

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
	}

	declaration.name = qualify(std::string(advance().text));
	declareName(declaration.name, "variable");

	_unit.globals.push_back(parseGlobalDeclarator(std::move(declaration)));
	recordConstant(_unit.globals.back(), false);

	// "constant float a = 1, b = a;" : the stars and brackets of the first name
	// stay with it, and each further name starts from the shared type.
	while (match(TokenKind::Comma)) {
		VariableDeclaration another;
		another.type = base;
		parseDeclaratorStars(another.type);
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
		}
		another.name = qualify(std::string(advance().text));
		declareName(another.name, "variable");
		_unit.globals.push_back(parseGlobalDeclarator(std::move(another)));
		recordConstant(_unit.globals.back(), false);
	}

	expect(TokenKind::Semicolon, "after a file-scope declaration");
}

// The array suffix and the required initialiser of one file-scope constant.
VariableDeclaration Parser::parseGlobalDeclarator(VariableDeclaration declaration) {
	bool deduceLength = false;
	if (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
		if (declaration.type.isPointer || declaration.type.arrayLength) {
			throw CompileError("an array of pointers is not lowered yet");
		}
		deduceLength = parseArrayDimensions(declaration.type);
	}

	if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
		const std::string what = "the function constant \"" + declaration.name + "\"";
		std::set<std::string> seen;
		parseAttributeList(seen, [&](const std::string& name, std::optional<uint32_t> argument, const std::string&) {
			if (name != "function_constant") {
				throw CompileError("attribute \"" + name + "\" does not apply to a file-scope constant");
			}
			if (!argument || *argument > kMaxFunctionConstantIndex) {
				throw CompileError(what + " needs an index from 0 to " + std::to_string(kMaxFunctionConstantIndex));
			}
			declaration.functionConstantIndex = *argument;
		});
		const Type& type = declaration.type;
		if (type.arrayLength || deduceLength) {
			throw CompileError(what + " is an array, and mslc lowers scalar function constants only");
		}
		if (type.isPointer || type.vectorWidth > 1 || type.isMatrix() || !type.namedType.empty()
			|| !type.enumName.empty() || type.resource != ResourceKind::None || type.isAtomic
			|| type.scalar == ScalarKind::Void || type.scalar == ScalarKind::Double) {
			throw CompileError(what + " has the type " + typeName(type) + ", and mslc lowers scalar function "
				"constants only");
		}
		if (at(TokenKind::Assign) || at(TokenKind::LBrace)) {
			throw CompileError(what + " has an initialiser, but its value is given when a function is made");
		}
		return declaration;
	}

	// Apple takes the brace list in place of "=", so "constant int k{5}" and
	// "constant int k = {5}" are the same declaration.
	if (!match(TokenKind::Assign) && !at(TokenKind::LBrace)) {
		throw CompileError("a constant declared at file scope needs an initialiser, found "
			+ std::string(tokenKindName(kind())));
	}

	declaration.initializer = parseInitializer();
	if (deduceLength) {
		deduceArrayLength(declaration);
	}

	return declaration;
}

// An initialiser, which is an expression or a braced list. A list is only
// spelled in an initialiser, so it is not a primary expression.
ExpressionPtr Parser::parseInitializer() {
	if (at(TokenKind::LBrace)) {
		return parseInitializerList();
	}

	return parseAssignment();
}

ExpressionPtr Parser::parseInitializerList() {
	const NestingScope scope(*this);
	auto list = std::make_unique<Expression>();
	list->kind = ExpressionKind::InitList;
	list->line = line();

	expect(TokenKind::LBrace, "to open a braced initialiser");

	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		InitializerElement element;

		// A struct's fields are written by name, ".direction = { ... }". A vector's
		// components are not: they are positional, like any other initialiser.
		if (at(TokenKind::Dot)) {
			advance();
			expectFieldName(element.fieldName);
			expect(TokenKind::Assign, "after a field name in a braced initialiser");
		}

		element.value = parseInitializer();
		list->elements.push_back(std::move(element));

		if (!match(TokenKind::Comma)) {
			break;
		}
	}

	expect(TokenKind::RBrace, "to close a braced initialiser");
	measure(*list);
	return list;
}


// "typedef", already consumed, then a type and the one name it is given. What can
// follow the type is deliberately narrow: a typedef of a pointer, an array or a
// function pointer is reported, because each would need a type the AST has no
// place for, and a typedef that declares several names is reported rather than
// read as the first.
void Parser::parseTypedef() {
	Type target;
	std::string enumTag;
	bool isEnum = false;
	bool anonymousStruct = false;
	std::optional<StructDecl> definedStruct;

	if (matchIdentifier("struct")) {
		std::string tag;
		if (kind() == TokenKind::Identifier) {
			tag = std::string(advance().text);
		}

		if (!tag.empty()) {
			QualifiedName spelled;
			spelled.parts = {tag};
			tag = at(TokenKind::LBrace) ? qualify(tag) : resolveName(spelled);
		}

		if (at(TokenKind::LBrace)) {
			StructDecl decl;
			decl.name = tag;
			parseStructBody(decl);
			anonymousStruct = tag.empty();
			if (!anonymousStruct) {
				declareName(tag, "struct");
			}
			definedStruct = std::move(decl);
		} else if (tag.empty() || !_unit.findStruct(tag)) {
			throw CompileError("typedef of the struct \"" + tag + "\", which is not declared");
		} else if (_declared[tag] == kAnonymousStructTypedef) {
			throw CompileError("the typedef \"" + tag + "\" cannot be referenced with a struct specifier");
		}

		target.namedType = tag;
	} else if (atKeyword("enum")) {
		bool hadBody = false;
		enumTag = parseEnumDeclaration(hadBody);
		if (!hadBody && !_enumTypes.count(enumTag)) {
			throw CompileError("typedef of the enum \"" + enumTag + "\", which is not declared");
		}
		isEnum = true;
	} else {
		target = parseType();
		if (target.isPointer) {
			throw CompileError("a typedef of a pointer type is not supported");
		}
		if (target.arrayLength && !target.isStdArray) {
			throw CompileError("a typedef of an array type is not supported");
		}
		if (target.addressSpace != AddressSpace::None) {
			throw CompileError("a typedef of a type with an address space is not supported");
		}
		if (!target.namedType.empty() && !_unit.findStruct(target.namedType)) {
			throw CompileError("typedef of the type \"" + target.namedType + "\", which is not declared");
		}
	}

	if (kind() != TokenKind::Identifier) {
		if (at(TokenKind::LParen)) {
			throw CompileError("a typedef of a function pointer is not supported");
		}
		if (at(TokenKind::Star)) {
			throw CompileError("a typedef of a pointer type is not supported");
		}
		throw CompileError("expected a typedef name, found " + std::string(tokenKindName(kind())));
	}

	const std::string name = qualify(std::string(advance().text));

	if (at(TokenKind::LBracket)) {
		throw CompileError("a typedef of an array type is not supported (\"" + name + "\")");
	}
	if (at(TokenKind::Comma)) {
		throw CompileError("a typedef that declares several names is not supported; split \"" + name
			+ "\" and the names after it into separate typedefs");
	}
	expect(TokenKind::Semicolon, "after a typedef");

	if (isEnum && enumTag.empty()) {
		declareName(name, "typedef");
		_enumTypes[name] = name;
		_enumUnderlying[name] = _lastEnumUnderlying;
		return;
	}

	if (isEnum) {
		const std::string tag = enumTag;
		const auto known = _enumTypes.find(name);
		if (known != _enumTypes.end()) {
			if (known->second != tag) {
				throw CompileError("typedef redefinition with different types ('" + known->second + "' vs '"
					+ tag + "')");
			}
			return;
		}

		if (name != enumTag) {
			declareName(name, "typedef");
		} else {
			_declared[name] = "typedef";
		}
		_enumTypes[name] = tag;
		return;
	}

	if (definedStruct) {
		if (anonymousStruct) {
			// The typedef name is the struct's own, as it is for a linkage name.
			declareName(name, kAnonymousStructTypedef);
			definedStruct->name = name;
		}
		_unit.structs.push_back(std::move(*definedStruct));
		finishStructFunctions(_unit.structs.back());
		if (anonymousStruct) {
			return;
		}
	}

	registerTypedef(name, target);
}

// The name is a typedef of target. A typedef under the name of the struct it names is the same
// type, but the name is then a typedef as well, which is what another declaration collides with.
void Parser::registerTypedef(const std::string& name, const Type& target) {
	if (name == target.namedType) {
		_declared[name] = "typedef";
		return;
	}

	Type existing;
	if (resolveTypeName(name, existing)) {
		if (!sameType(existing, target)) {
			const auto spelled = [](Type type) {
				if (!type.enumName.empty()) {
					type.namedType = type.enumName;
				}
				return typeName(type);
			};
			throw CompileError("typedef redefinition with different types ('" + spelled(target) + "' vs '"
				+ spelled(existing) + "')");
		}
		return;
	}

	declareName(name, "typedef");
	_typedefs[name] = target;
}

void Parser::seedStdlibEnums() {
	_enumTypes["metal::component"] = "metal::component";
	_enumUnderlying["metal::component"] = ScalarKind::Int;
	_enumConstants["metal::component::x"] = { 0, ScalarKind::Int, "metal::component" };
	_enumConstants["metal::component::y"] = { 1, ScalarKind::Int, "metal::component" };
	_enumConstants["metal::component::z"] = { 2, ScalarKind::Int, "metal::component" };
	_enumConstants["metal::component::w"] = { 3, ScalarKind::Int, "metal::component" };
}

} // namespace mslc
