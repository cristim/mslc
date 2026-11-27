#include "parser/internal.h"

namespace mslc {

void Parser::expectFieldName(std::string& out) {
	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a field name, found " + std::string(tokenKindName(kind())));
	}

	out = std::string(advance().text);
}

bool Parser::parseAddressSpace(AddressSpace& space) {
	const auto found = addressSpaceFor(current().text);
	if (!found) {
		return false;
	}

	space = *found;
	advance();
	return true;
}

// The highest index Apple's compiler accepts for an attribute that takes one:
// 'buffer' attribute parameter is out of bounds: must be between 0 and 30.
void checkAttributeIndex(const std::string& name, uint32_t index) {
	const uint32_t limit = name == "texture" ? 127 : name == "sampler" ? 15 : 30;
	if (index > limit) {
		throw CompileError("attribute \"" + name + "\" index " + std::to_string(index)
			+ " is out of bounds: it must be between 0 and " + std::to_string(limit));
	}
}

void Parser::deduceArrayLength(VariableDeclaration& declaration) {
	const Expression* list = declaration.initializer.get();
	if (!list || list->kind != ExpressionKind::InitList || list->elements.empty()) {
		throw CompileError("the array \"" + declaration.name + "\" has no size and needs an initialiser list");
	}
	if (!declaration.type.innerArrayLengths.empty()) {
		for (const InitializerElement& element: list->elements) {
			if (element.value->kind != ExpressionKind::InitList) {
				throw CompileError("the first length of \"" + declaration.name + "\" is deduced from braced rows; "
					"write a row in braces, or give the length");
			}
		}
	}
	declaration.type.arrayLength = static_cast<uint32_t>(list->elements.size());
}

bool Parser::parseArrayDimensions(Type& type) {
	bool deduced = false;
	bool outermost = true;
	if (type.arrayLength && !type.isStdArray && at(TokenKind::LBracket)) {
		throw CompileError("an array of an array alias is not supported");
	}
	while (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
		advance();
		if (at(TokenKind::RBracket)) {
			if (!outermost) {
				throw CompileError("only the outermost array length may be omitted");
			}
			deduced = true;
		} else {
			const int64_t length = parseConstantIndex("an array length");
			if (length <= 0) {
				throw CompileError("zero-length arrays are not permitted in C++");
			}
			if (outermost) {
				type.arrayLength = static_cast<uint32_t>(length);
			} else {
				type.innerArrayLengths.push_back(static_cast<uint32_t>(length));
			}
		}
		outermost = false;
		expect(TokenKind::RBracket, "to close an array length");
	}
	return deduced;
}

std::optional<uint32_t> Parser::tryParseArrayLength() {
	if (!at(TokenKind::LBracket) || lookahead().kind == TokenKind::LBracket) {
		return std::nullopt;
	}

	advance();

	const auto length = static_cast<uint32_t>(parseConstantIndex("an array length"));
	expect(TokenKind::RBracket, "to close an array length");
	return length;
}

// One type qualifier or address space at the cursor, applied to the type. MSL
// takes them on either side of the type name, so "In const in", "float const *p"
// and "const In in" are the same declaration. Before a "*" they qualify the
// type; after one they qualify the pointer, which none of the flags here track.
bool Parser::parseQualifier(Type& type, bool afterPointer) {
	if (kind() != TokenKind::Identifier) {
		return false;
	}

	const std::string_view text = current().text;

	AddressSpace space;
	if (!afterPointer && addressSpaceFor(text)) {
		parseAddressSpace(space);
		// A different second address space is a mistake rather than an
		// alternative spelling, and which one was meant decides which storage
		// class a binding lands in, so it is reported rather than resolved.
		if (type.addressSpace != AddressSpace::None && type.addressSpace != space) {
			throw CompileError("mslc does not support more than one address space on a type, found \""
				+ std::string(text) + "\" after another (Apple accepts this)");
		}

		type.addressSpace = space;
		return true;
	}

	if (!isTypeQualifier(text)) {
		return false;
	}

	if (text == "const" && !afterPointer) {
		type.isConst = true;
	}
	if (text == "volatile" && !afterPointer) {
		type.isVolatile = true;
	}
	if (text == "constexpr") {
		type.isConstexpr = true;
	}
	if (text == "static") {
		type.isStatic = true;
	}

	advance();
	return true;
}

// Reads a run of integer specifier words, with const and volatile among them,
// into type.scalar. Apple takes the words in any order and the same signedness
// or "short" twice with only a warning, so those are accepted; "long long" is
// not supported by Metal, and short, long, char and int combine only as C++ allows.
// A run of one word is "unsigned" or "signed" alone (an int) or a single-token name.
size_t Parser::parseSpecifierRun(Type& type) {
	size_t unsignedCount = 0, signedCount = 0, shortCount = 0, longCount = 0, intCount = 0, charCount = 0;
	std::string spelling;
	size_t words = 0;

	while (kind() == TokenKind::Identifier) {
		const std::string_view text = current().text;
		if (isIntegerSpecifier(text)) {
			++words;
			spelling += (spelling.empty() ? "" : " ") + std::string(text);
			if (text == "unsigned") {
				++unsignedCount;
			} else if (text == "signed") {
				++signedCount;
			} else if (text == "short") {
				++shortCount;
			} else if (text == "long") {
				++longCount;
			} else if (text == "int") {
				++intCount;
			} else {
				++charCount;
			}
			advance();
		} else if (text == "const" || text == "volatile") {
			parseQualifier(type, false);
		} else {
			break;
		}
	}

	const auto invalid = [&](const std::string& why) {
		return CompileError("\"" + spelling + "\" cannot be combined: " + why);
	};

	// A bare "int" or "char" may be followed by the name a typedef declares, which
	// can be a builtin's, so only a run that takes a modifier is checked for a
	// type name of its own after it.
	ScalarKind following;
	if (words > intCount + charCount && kind() == TokenKind::Identifier && isScalarTypeName(current().text, following)) {
		throw invalid("\"" + std::string(current().text) + "\" is a complete type name of its own");
	}
	if (unsignedCount > 0 && signedCount > 0) {
		throw invalid("a type is either signed or unsigned");
	}
	if (longCount > 1) {
		throw CompileError("\"" + spelling + "\": long long is not supported in Metal");
	}
	if (shortCount > 0 && longCount > 0) {
		throw invalid("short and long exclude each other");
	}
	if (charCount > 0 && (shortCount > 0 || longCount > 0 || intCount > 0 || charCount > 1)) {
		throw invalid("char takes only signed or unsigned");
	}
	if (intCount > 1) {
		throw invalid("int appears twice");
	}

	const bool isUnsigned = unsignedCount > 0;
	if (charCount > 0) {
		type.scalar = isUnsigned ? ScalarKind::UChar : ScalarKind::Char;
	} else if (shortCount > 0) {
		type.scalar = isUnsigned ? ScalarKind::UShort : ScalarKind::Short;
	} else if (longCount > 0) {
		type.scalar = isUnsigned ? ScalarKind::ULong : ScalarKind::Long;
	} else {
		type.scalar = isUnsigned ? ScalarKind::UInt : ScalarKind::Int;
	}

	return words;
}

// The stars of one declarator. A star binds to the name it precedes, so in
// "float *p, q;" the second declarator starts again from the type before the stars.
void Parser::parseDeclaratorStars(Type& type) {
	while (at(TokenKind::Star)) {
		if (type.isPointer) {
			throw CompileError("a pointer to a pointer is not supported");
		}
		advance();
		type.isPointer = true;

		while (parseQualifier(type, true)) {
		}
	}

	// constexpr makes the variable const, and on a pointer that is the pointer
	// rather than what it points at, which is what isConst means there.
	if (type.isConstexpr && !type.isPointer) {
		type.isConst = true;
	}
}

bool Parser::atStdArray(size_t& nameTokens) const {
	if (kind() != TokenKind::Identifier && !at(TokenKind::ColonColon)) {
		return false;
	}
	// Only a name that could be the library's array is resolved: resolving any other one here would
	// report an ambiguity the caller handles itself.
	const Token& first = at(TokenKind::ColonColon) ? lookahead() : current();
	if (first.kind != TokenKind::Identifier || (first.text != "array" && first.text != "metal")) {
		return false;
	}
	const std::string name = peekResolved(nameTokens);
	if (name != "metal::array" && (name != "array" || _declared.count("array"))) {
		return false;
	}
	return lookahead(nameTokens).kind == TokenKind::Less;
}

// metal::array<T, N> is a struct holding T[N] in Apple's library: it lays out as that array and
// indexes like it, and unlike it, a copy, an assignment, a by-value parameter and a return carry
// the elements. It is read as the array with isStdArray set, an array of arrays as a
// multi-dimensional one. The qualifiers read before it apply to the whole array.
void Parser::parseStdArray(Type& type, size_t nameTokens) {
	for (size_t i = 0; i < nameTokens; ++i) {
		advance();
	}
	expect(TokenKind::Less, "to open the template arguments of array");
	const Type qualifiers = type;
	Type element = parseType(false, false);
	if (element.isPointer || element.isConst || element.isConstexpr || element.isStatic
		|| element.addressSpace != AddressSpace::None || element.isAtomic || element.isPacked
		|| element.resource != ResourceKind::None || !element.enumName.empty()
		|| (element.arrayLength && !element.isStdArray)) {
		throw CompileError("array<T, N> with an element that is a pointer, a reference, const, an atomic, a "
			"packed vector, a resource, an enum or in an address space is not lowered");
	}
	expect(TokenKind::Comma, "between the element type and the length of array");
	const ExpressionPtr length = parseAdditive();
	if (!at(TokenKind::Greater)) {
		throw CompileError("this array length is not lowered: a constant that is more than an arithmetic "
			"expression needs parentheses");
	}
	advance();
	const int64_t count = length->kind == ExpressionKind::BoolLiteral ? (length->boolValue ? 1 : 0)
		: evaluateConstant(*length, false);
	if (count <= 0 || count > UINT32_MAX) {
		throw CompileError("array<T, " + std::to_string(count) + "> is not lowered: the length has to be "
			"between 1 and 4294967295 (Apple accepts array<T, 0> and gives it the size of one element)");
	}
	if (element.arrayLength) {
		element.innerArrayLengths.insert(element.innerArrayLengths.begin(), *element.arrayLength);
	}
	element.arrayLength = static_cast<uint32_t>(count);
	element.isStdArray = true;
	element.isConst = qualifiers.isConst;
	element.isConstexpr = qualifiers.isConstexpr;
	element.isStatic = qualifiers.isStatic;
	element.addressSpace = qualifiers.addressSpace;
	type = element;
}

bool Parser::atAsType(size_t& nameTokens) const {
	if (kind() != TokenKind::Identifier && !at(TokenKind::ColonColon)) {
		return false;
	}
	// metal::as_type through a namespace alias of metal, "namespace m = metal; m::as_type<T>(x)".
	if (const size_t prefix = libraryPrefixTokens(); prefix && lookahead(prefix).kind == TokenKind::Identifier
		&& lookahead(prefix).text == "as_type" && lookahead(prefix + 1).kind == TokenKind::Less) {
		nameTokens = prefix + 1;
		return true;
	}
	const Token& first = at(TokenKind::ColonColon) ? lookahead() : current();
	if (first.kind != TokenKind::Identifier || (first.text != "as_type" && first.text != "metal")) {
		return false;
	}
	// A local or a member of the enclosing struct named as_type hides the library's, unless qualified.
	if (kind() == TokenKind::Identifier && current().text == "as_type" && lookahead().kind != TokenKind::ColonColon
		&& (isLocal("as_type") || memberHidesName("as_type"))) {
		return false;
	}
	const std::string name = peekResolved(nameTokens);
	if (name != "metal::as_type" && (name != "as_type" || _declared.count("as_type"))) {
		return false;
	}
	return lookahead(nameTokens).kind == TokenKind::Less;
}

// as_type<T>(x) gives x's bits as a T. It is read as a call of the builtin kAsTypeCallee with T as
// the template argument, so that it is lowered as a reinterpretation, never as the conversion a
// constructor or a cast makes.
ExpressionPtr Parser::parseAsType(size_t nameTokens) {
	const size_t atLine = line();
	for (size_t i = 0; i < nameTokens; ++i) {
		advance();
	}
	expect(TokenKind::Less, "to open the template argument of as_type");
	TemplateArgument target;
	target.type = parseType(false, false);
	expect(TokenKind::Greater, "to close the template argument of as_type");
	expect(TokenKind::LParen, "for the operand of as_type");
	auto call = std::make_unique<Expression>();
	call->kind = ExpressionKind::Call;
	call->line = atLine;
	call->left = makeIdentifier(kAsTypeCallee, atLine);
	call->left->hasTemplateArguments = true;
	call->left->templateArguments.push_back(std::move(target));
	call->arguments = parseArgumentList("to close the operand of as_type");
	measure(*call);
	return call;
}

bool Parser::atLibraryVec(size_t& nameTokens) const {
	if (kind() != TokenKind::Identifier && !at(TokenKind::ColonColon)) {
		return false;
	}
	const Token& first = at(TokenKind::ColonColon) ? lookahead() : current();
	if (first.kind != TokenKind::Identifier || (first.text != "vec" && first.text != "metal")) {
		return false;
	}
	// A local or a member of the enclosing struct named vec hides the library's, unless qualified.
	if (kind() == TokenKind::Identifier && current().text == "vec" && lookahead().kind != TokenKind::ColonColon
		&& (isLocal("vec") || memberHidesName("vec"))) {
		return false;
	}
	const std::string name = peekResolved(nameTokens);
	if (name != "metal::vec" && (name != "vec" || _declared.count("vec"))) {
		return false;
	}
	return lookahead(nameTokens).kind == TokenKind::Less;
}

// metal::vec<T, N> is the library's name for the vector of N T: vec<float, 4> is float4, so it is
// read as that vector. The qualifiers read before it apply to the vector.
void Parser::parseLibraryVec(Type& type, size_t nameTokens) {
	for (size_t i = 0; i < nameTokens; ++i) {
		advance();
	}
	expect(TokenKind::Less, "to open the template arguments of vec");
	const Type element = parseType(false, false);
	// A function template's signature is read to deduce its arguments with each type parameter left to
	// deduce as a marker, "<T>": vec<T, N> is then the marker with the vector's width, which deduction
	// takes apart.
	const bool marker = element.namedType.size() > 2 && element.namedType.front() == '<'
		&& element.namedType.back() == '>';
	if (element.isPointer || element.isConst || element.isConstexpr || element.isStatic
		|| element.addressSpace != AddressSpace::None || element.isAtomic || element.isPacked || element.isVector()
		|| element.isMatrix() || element.arrayLength || element.resource != ResourceKind::None
		|| (!element.namedType.empty() && !marker) || !element.enumName.empty()
		|| (!marker && (element.scalar == ScalarKind::Void || element.scalar == ScalarKind::Double))) {
		throw CompileError("vec<T, N> takes a scalar type, bool or one of the integer, half and float types, as T");
	}
	expect(TokenKind::Comma, "between the element type and the length of vec");
	const ExpressionPtr length = parseAdditive();
	if (!at(TokenKind::Greater)) {
		throw CompileError("this vec length is not lowered: a constant that is more than an arithmetic "
			"expression needs parentheses");
	}
	advance();
	if (length->kind == ExpressionKind::Identifier && length->name.rfind("<template ", 0) == 0
		&& !_enumConstants.count(length->name)) {
		throw CompileError("deducing the length N of vec<T, N> from an argument is not lowered; give the template "
			"argument explicitly");
	}
	const int64_t count = evaluateConstant(*length, false);
	if (count < 2 || count > 4) {
		throw CompileError("vec<T, " + std::to_string(count) + "> is not lowered: Metal's vectors have 2, 3 or 4 "
			"components");
	}
	type.scalar = element.scalar;
	type.namedType = element.namedType;
	type.vectorWidth = static_cast<uint32_t>(count);
}

Type Parser::parseType(bool allowResource, bool allowAuto, Type* base) {
	Type type;
	const size_t typenameAt = _position;
	bool traitType = false;

	// MSL writes the address space and the const qualifier in either order, and
	// both orders are ordinary: "device const float*" and "const device Vertex *"
	// are the same declaration spelled two ways. Reading them as two fixed
	// sequences took "const" as the whole prefix and then read "device" as a type
	// name, so the parameter became "Vertex" and the list ended at the "*".
	while (parseQualifier(type, false)) {
	}
	const bool prefixQualified = type.isConst || type.addressSpace != AddressSpace::None;

	// metal::sampler and metal::texture2d, which Apple takes as the unqualified names.
	if (kind() == TokenKind::Identifier && current().text == "metal" && lookahead().kind == TokenKind::ColonColon
		&& lookahead(2).kind == TokenKind::Identifier && isResourceTypeName(lookahead(2).text)) {
		advance();
		advance();
	} else if (const size_t prefix = libraryResourcePrefixTokens()) {
		for (size_t i = 0; i < prefix; ++i) {
			advance();
		}
	}

	// "typename" before a name only ever introduces a trait's type ("typename enable_if<C>::type").
	const bool typenameKeyword = atKeyword("typename");
	if (typenameKeyword) {
		advance();
	}
	size_t nameTokens = 0;
	std::string name;
	if (kind() == TokenKind::Identifier || at(TokenKind::ColonColon)) {
		name = peekResolved(nameTokens);
	}
	const auto consumeName = [&] {
		for (size_t i = 0; i < nameTokens; ++i) {
			advance();
		}
	};
	std::string library = name.empty() ? std::string() : libraryName(name);
	traitType = (library == "enable_if" || library == "enable_if_t" || library == "make_scalar"
		|| library == "make_scalar_t") && lookahead(nameTokens).kind == TokenKind::Less;
	if (typenameKeyword && !traitType) {
		_position = typenameAt;
		throw CompileError("a dependent type written with \"typename\" is not lowered");
	}

	if (traitType) {
		consumeName();
		parseTraitType(library, type);
	} else if (kind() == TokenKind::Identifier && current().text == "auto") {
		if (!allowAuto) {
			throw CompileError("\"auto\" is only lowered for a local variable, where the initialiser gives the type");
		}
		advance();
		type.isAuto = true;
	} else if (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text)) {
		parseSpecifierRun(type);
	} else if (kind() == TokenKind::Identifier && nameTokens == 1 && isResourceTypeName(current().text)) {
		if (!allowResource) {
			throw CompileError("\"" + std::string(current().text) + "\" is a texture or sampler type, "
				"which mslc takes as an entry point parameter only, and a sampler also as a local");
		}
		parseResourceType(type);
	} else if (size_t arrayTokens = 0; atStdArray(arrayTokens)) {
		parseStdArray(type, arrayTokens);
	} else if (size_t vecTokens = 0; atLibraryVec(vecTokens)) {
		parseLibraryVec(type, vecTokens);
	} else if (!name.empty() && resolveTypeName(name, type)) {
		if (type.isPointer && prefixQualified) {
			throw CompileError("a const or an address space written before a pointer alias is not supported; "
				"put it in the alias");
		}
		if (type.resource != ResourceKind::None && !allowResource) {
			throw CompileError("\"" + std::string(current().text) + "\" is the type " + typeName(type) + ", a texture or sampler type, which "
				"mslc takes as a parameter only, and a sampler also as a local");
		}
		consumeName();
	} else if (atKeyword("enum")) {
		advance();
		if (kind() != TokenKind::Identifier && !at(TokenKind::ColonColon)) {
			throw CompileError("an elaborated enum type must name an enum declared earlier in the unit");
		}
		const std::string tag = peekResolved(nameTokens);
		if (!_enumTypes.count(tag)) {
			throw CompileError("an elaborated enum type must name an enum declared earlier in the unit");
		}
		resolveTypeName(tag, type);
		consumeName();
	} else if (!name.empty()) {
		type.namedType = name;
		consumeName();
	} else {
		throw CompileError("expected a type, found " + std::string(tokenKindName(kind()))
			+ " \"" + std::string(current().text) + "\"");
	}

	const bool aliasPointer = type.isPointer;
	const bool aliasArray = type.arrayLength && !type.isStdArray;
	const bool constBefore = type.isConst;
	while (parseQualifier(type, false)) {
	}
	if (aliasPointer && type.isConst != constBefore) {
		throw CompileError("a const written after a pointer alias is not supported; put it in the alias");
	}

	if (base) {
		*base = type;
	}
	parseDeclaratorStars(type);
	if (aliasArray && (type.isPointer || at(TokenKind::LBracket))) {
		throw CompileError("a pointer to an array alias, and an array of one, are not supported");
	}

	if (type.isStdArray && at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
		throw CompileError("an array of array<T, N> is not lowered");
	}
	if (auto length = tryParseArrayLength()) {
		type.arrayLength = length;
	}

	return type;
}

void Parser::parseResourceType(Type& type) {
	const std::string name(advance().text);

	if (name == "sampler") {
		type.resource = ResourceKind::Sampler;
		return;
	}

	static const std::map<std::string_view, ResourceKind> depthTextures = {
		{ "depth2d", ResourceKind::Texture2D },
		{ "depth2d_array", ResourceKind::Texture2DArray },
		{ "depthcube", ResourceKind::TextureCube },
		{ "depthcube_array", ResourceKind::TextureCubeArray },
	};
	static const std::map<std::string_view, ResourceKind> textures = {
		{ "texture1d", ResourceKind::Texture1D },
		{ "texture2d", ResourceKind::Texture2D },
		{ "texturecube", ResourceKind::TextureCube },
		{ "texture3d", ResourceKind::Texture3D },
		{ "texture2d_array", ResourceKind::Texture2DArray },
		{ "texture1d_array", ResourceKind::Texture1DArray },
		{ "texturecube_array", ResourceKind::TextureCubeArray },
		{ "texture_buffer", ResourceKind::TextureBuffer },
	};
	struct Multisampled { ResourceKind kind; bool depth; };
	static const std::map<std::string_view, Multisampled> multisampled = {
		{ "texture2d_ms", { ResourceKind::Texture2D, false } },
		{ "texture2d_ms_array", { ResourceKind::Texture2DArray, false } },
		{ "depth2d_ms", { ResourceKind::Texture2D, true } },
		{ "depth2d_ms_array", { ResourceKind::Texture2DArray, true } },
	};
	const auto depth = depthTextures.find(name);
	const auto color = textures.find(name);
	const auto ms = multisampled.find(name);
	const bool isMultisampled = ms != multisampled.end();
	const bool isDepth = depth != depthTextures.end() || (isMultisampled && ms->second.depth);
	if (!isMultisampled && depth == depthTextures.end() && color == textures.end()) {
		throw CompileError("\"" + name + "\" is not lowered yet; mslc lowers texture1d, texture2d, texturecube, "
			"texture3d, texture2d_array, texture1d_array, texturecube_array and texture_buffer, depth2d, depth2d_array, depthcube and "
			"depthcube_array, texture2d_ms, texture2d_ms_array, depth2d_ms and depth2d_ms_array, and sampler");
	}

	expect(TokenKind::Less, ("to open the sampled type of " + name).c_str());
	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected the sampled type of " + name + ", found "
			+ std::string(tokenKindName(kind())));
	}

	std::string component(current().text);
	// A function template's type parameter while its signature is read to deduce it: the texture keeps the
	// parameter's marker, from which deduction takes the argument texture's sampled type.
	std::string deducedFrom;
	if (isIntegerSpecifier(component)) {
		Type channel;
		parseSpecifierRun(channel);
		component = typeName(channel);
	} else {
		advance();
		// A template's type parameter is reached through the name its block binds.
		const std::string spelled = component;
		if (const std::string* bound = blockName(component); bound && !bound->empty() && _typedefs.count(*bound)) {
			component = *bound;
		}
		Type alias;
		if (!_enumTypes.count(component) && resolveTypeName(component, alias) && !alias.isVector() && !alias.isMatrix() && alias.namedType.empty()) {
			alias.isConst = false;
			component = typeName(alias);
		} else if (!_enumTypes.count(component) && resolveTypeName(component, alias) && isDeductionMarker(alias)
			&& !alias.isVector() && !alias.isMatrix() && !alias.isPointer && !alias.arrayLength) {
			deducedFrom = alias.namedType;
			component = "float";
		} else {
			component = spelled;
		}
	}
	static const std::map<std::string_view, ScalarKind> elements = {
		{ "float", ScalarKind::Float }, { "half", ScalarKind::Half }, { "int", ScalarKind::Int },
		{ "uint", ScalarKind::UInt }, { "short", ScalarKind::Short }, { "ushort", ScalarKind::UShort },
	};
	const auto element = elements.find(component);
	if (element == elements.end()) {
		throw CompileError(name + "<" + component + "> is not lowered yet; mslc lowers the "
			"sampled types float, half, int, uint, short and ushort");
	}

	// Apple's texture_buffer and multisampled textures are access::read unless spelled otherwise.
	const bool isBuffer = color != textures.end() && color->second == ResourceKind::TextureBuffer;
	std::string access = (isBuffer || isMultisampled) ? "read" : "sample";
	bool accessSpelled = false;
	if (match(TokenKind::Comma)) {
		if (kind() == TokenKind::Identifier && current().text == "metal" && lookahead().kind == TokenKind::ColonColon) {
			// A nearer namespace named metal supplies its own "access", not the library's (a file
			// that reopens namespace metal itself has not shadowed anything).
			QualifiedName qualifier;
			qualifier.parts.push_back(std::string(current().text));
			if (const std::string nearer = namespaceOf(qualifier, 1, NamespaceLookup::Qualifier);
			!nearer.empty() && nearer != kStdlibNamespace) {
				throw CompileError("\"metal::access\" names \"" + nearer + "::access\" here, not the library's "
					"metal::access, which a texture's access qualifier has to be");
			}
			advance();
			advance();
		} else if (const size_t prefix = libraryPrefixTokens();
			prefix && lookahead(prefix).kind == TokenKind::Identifier && lookahead(prefix).text == "access") {
			for (size_t i = 0; i < prefix; ++i) {
				advance();
			}
		}
		if (kind() != TokenKind::Identifier || current().text != "access") {
			throw CompileError("expected an access qualifier after the sampled type of " + name);
		}
		advance();
		expect(TokenKind::ColonColon, "after \"access\"");
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected an access qualifier name after \"access::\"");
		}
		access = std::string(advance().text);
		accessSpelled = true;
		if (access != "sample" && access != "read" && access != "write" && access != "read_write") {
			throw CompileError(name + " access::" + access + " is not lowered yet; mslc lowers "
				"access::sample, which is the default, access::read, access::write and access::read_write");
		}
	}

	expect(TokenKind::Greater, ("to close the sampled type of " + name).c_str());
	if (isBuffer && access == "sample") {
		throw CompileError("texture_buffer<" + component + ", access::sample> is invalid: a texture_buffer has no "
			"sampler, and takes access::read, access::write or access::read_write");
	}
	type.resource = isMultisampled ? ms->second.kind : depth != depthTextures.end() ? depth->second : color->second;
	type.isDepth = isDepth;
	type.isMultisampled = isMultisampled;
	if (type.isDepth && element->second != ScalarKind::Float) {
		throw CompileError(name + "<" + component + (accessSpelled ? ", access::" + access : "")
			+ "> is not lowered yet; mslc lowers depth textures of float (Apple has no depth half)");
	}
	if (isMultisampled && access != "read" && !(isDepth && access == "sample")) {
		throw CompileError(name + " access::" + access + " is not lowered yet; a multisampled texture is read-only");
	}
	if (type.isDepth && access != "sample" && access != "read") {
		throw CompileError(name + "<" + component + ", access::" + access
			+ "> is not lowered yet; mslc lowers depth textures of float with access::sample or access::read");
	}
	type.textureAccess = access == "write" ? TextureAccess::Write
		: access == "read" ? TextureAccess::Read
		: access == "read_write" ? TextureAccess::ReadWrite : TextureAccess::Sample;
	type.scalar = element->second;
	type.namedType = deducedFrom;
}

} // namespace mslc
