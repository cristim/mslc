#include "parser.h"

#include <climits>
#include <unordered_map>

namespace mslc {

namespace {

	// Bare type keywords, mapped to their scalar kind. MSL spells these
	// without a typedef, so they are lexed as identifiers and recognised here.
	const std::unordered_map<std::string_view, ScalarKind>& scalarTypeNames() {
		static const std::unordered_map<std::string_view, ScalarKind> table = {
			{ "void", ScalarKind::Void },
			{ "bool", ScalarKind::Bool },
			{ "char", ScalarKind::Char },
			{ "uchar", ScalarKind::UChar },
			{ "ushort", ScalarKind::UShort },
			{ "short", ScalarKind::Short },
			{ "uint", ScalarKind::UInt },
			{ "int", ScalarKind::Int },
			{ "ulong", ScalarKind::ULong },
			{ "long", ScalarKind::Long },
			{ "half", ScalarKind::Half },
			{ "float", ScalarKind::Float },
			{ "double", ScalarKind::Double },

			// The fixed-width typedefs MSL supplies. Blender's shaders use these
			// spellings without including a header, since MSL makes <cstdint>
			// available on its own.
			{ "int8_t", ScalarKind::Char },
			{ "uint8_t", ScalarKind::UChar },
			{ "int16_t", ScalarKind::Short },
			{ "uint16_t", ScalarKind::UShort },
			{ "int32_t", ScalarKind::Int },
			{ "uint32_t", ScalarKind::UInt },
			{ "int64_t", ScalarKind::Long },
			{ "uint64_t", ScalarKind::ULong },
		};

		return table;
	}

	bool isScalarTypeName(std::string_view text, ScalarKind& outKind) {
		const auto& table = scalarTypeNames();
		auto it = table.find(text);
		if (it == table.end()) {
			return false;
		}

		outKind = it->second;
		return true;
	}

	// A scalar, vector or matrix type name, filling in the scalar, the vector
	// width and the matrix column count. Metal spells a vector as its scalar type
	// with the component count on the end, so "float4" is "float" and a 4, and
	// the only thing to do with the name is split it. The count has to be 2 to 9
	// and the rest has to name a scalar: a type with a width of 1 is not spelled
	// that way, and "float0" is not a type. A matrix is "floatCxR" or "halfCxR"
	// with C and R from 2 to 4; MSL has no matrix of any other scalar.
	bool isUnpackedTypeName(std::string_view text, Type& outType) {
		ScalarKind kind;
		if (isScalarTypeName(text, kind)) {
			outType.scalar = kind;
			return true;
		}

		if (text.size() < 2) {
			return false;
		}

		const char last = text.back();
		if (last < '2' || last > '9') {
			return false;
		}

		if (text.size() > 3 && text[text.size() - 2] == 'x') {
			const char columns = text[text.size() - 3];
			if (columns < '2' || columns > '4' || last > '4'
				|| !isScalarTypeName(text.substr(0, text.size() - 3), kind)
				|| (kind != ScalarKind::Float && kind != ScalarKind::Half)) {
				return false;
			}

			outType.scalar = kind;
			outType.vectorWidth = static_cast<uint32_t>(last - '0');
			outType.matrixColumns = static_cast<uint32_t>(columns - '0');
			return true;
		}

		if (!isScalarTypeName(text.substr(0, text.size() - 1), kind)) {
			return false;
		}

		outType.scalar = kind;
		outType.vectorWidth = static_cast<uint32_t>(last - '0');
		return true;
	}

	// packed_float3 and its family: packed_<scalar><N> for the integer, half and
	// float scalars and N from 2 to 4. Apple also spells packed_bool, which mslc
	// reports by name where the type is resolved, and has no packed double.
	bool isPackedVectorName(std::string_view text, Type& outType) {
		constexpr std::string_view prefix = "packed_";
		if (text.size() <= prefix.size() || text.substr(0, prefix.size()) != prefix) {
			return false;
		}

		Type vector;
		if (!isUnpackedTypeName(text.substr(prefix.size()), vector) || !vector.isVector()
			|| vector.isMatrix() || vector.vectorWidth > 4 || vector.scalar == ScalarKind::Double) {
			return false;
		}

		outType.scalar = vector.scalar;
		outType.vectorWidth = vector.vectorWidth;
		outType.isPacked = true;
		return true;
	}

	bool isTypeName(std::string_view text, Type& outType) {
		return isUnpackedTypeName(text, outType) || isPackedVectorName(text, outType);
	}

	// The vector typedefs Apple's compiler has without any include: vector_float4,
	// simd_uint2 and the rest. They name a vector of 2 to 4 components only, so
	// vector_float and vector_float4x4 are not types, and neither are the double
	// and 8 or 16 wide ones, which Apple declares as incomplete.
	bool isSimdVectorName(std::string_view text, Type& outType) {
		for (const std::string_view prefix: { std::string_view("vector_"), std::string_view("simd_") }) {
			if (text.size() > prefix.size() && text.substr(0, prefix.size()) == prefix) {
				Type vector;
				if (isUnpackedTypeName(text.substr(prefix.size()), vector) && vector.isVector() && !vector.isMatrix()
					&& vector.vectorWidth <= 4 && vector.scalar != ScalarKind::Double
					&& vector.scalar != ScalarKind::Void) {
					outType.scalar = vector.scalar;
					outType.vectorWidth = vector.vectorWidth;
					return true;
				}
			}
		}

		return false;
	}

	// Whether a name is a scalar, vector or matrix type, for a caller that only
	// needs to know which it is.
	bool isTypeName(std::string_view text) {
		Type type;
		return isTypeName(text, type);
	}

	// Keywords that may appear before a type and are not address spaces.
	bool isTypeQualifier(std::string_view text) {
		return text == "const" || text == "static" || text == "constexpr"
			|| text == "volatile" || text == "restrict" || text == "__restrict";
	}

	// The texture and sampler type names. Only texture2d and sampler are lowered;
	// the rest are recognised so they are reported by name rather than read as a
	// struct type and failing later.
	bool isResourceTypeName(std::string_view text) {
		static const std::set<std::string_view> names = {
			"sampler", "texture1d", "texture1d_array", "texture2d", "texture2d_array",
			"texture2d_ms", "texture2d_ms_array", "texture3d", "texturecube",
			"texturecube_array", "texture_buffer", "depth2d", "depth2d_array",
			"depth2d_ms", "depth2d_ms_array", "depthcube", "depthcube_array",
		};
		return names.count(text) > 0;
	}

	// MSL builtin attributes the subset can map onto a SPIR-V BuiltIn. An
	// attribute outside this table is rejected rather than ignored, so an
	// unsupported builtin can never be dropped without a diagnostic.
	std::optional<ParameterAttributes::Builtin> builtinFromName(std::string_view name) {
		using Builtin = ParameterAttributes::Builtin;

		if (name == "thread_position_in_grid") { return Builtin::ThreadPositionInGrid; }
		if (name == "threadgroup_position_in_grid") { return Builtin::ThreadgroupPositionInGrid; }
		if (name == "thread_position_in_threadgroup") { return Builtin::ThreadPositionInThreadgroup; }
		if (name == "thread_index_in_threadgroup") { return Builtin::ThreadIndexInThreadgroup; }
		if (name == "vertex_id") { return Builtin::VertexID; }
		if (name == "instance_id") { return Builtin::InstanceID; }
		if (name == "position") { return Builtin::Position; }
		if (name == "frag_coord") { return Builtin::FragCoord; }
		if (name == "front_facing") { return Builtin::FrontFacing; }

		return std::nullopt;
	}

	// Cast targets the subset accepts in an explicit cast expression.
	bool isCastTypeName(std::string_view name) {
		return scalarTypeNames().count(name) > 0;
	}

	// The address space a name spells, or nothing when it spells none. Shared by
	// the type parser and the top-level declaration dispatch, so the two agree on
	// which names are address spaces.
	std::optional<AddressSpace> addressSpaceFor(std::string_view name) {
		if (name == "device") { return AddressSpace::Device; }
		if (name == "constant") { return AddressSpace::Constant; }
		if (name == "threadgroup") { return AddressSpace::Threadgroup; }
		if (name == "thread") { return AddressSpace::Thread; }

		return std::nullopt;
	}

}

bool isMSLBuiltinName(std::string_view name) {
	static const char* const names[] = {
		"thread_position_in_grid",
		"threadgroup_position_in_grid",
		"thread_position_in_threadgroup",
		"thread_index_in_threadgroup",
		"vertex_id",
		"instance_id",
		"position",
		"frag_coord",
		"front_facing",
		"threadgroups_per_grid",
		"threads_per_threadgroup",
	};

	for (const char* candidate: names) {
		if (name == candidate) {
			return true;
		}
	}

	return false;
}

const Token& Parser::lookahead(size_t offset) const {
	const size_t index = _position + offset;
	return index < _tokens.size() ? _tokens[index] : _tokens.back();
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

TranslationUnit Parser::parse() {
	while (!at(TokenKind::EndOfFile)) {
		parseDeclaration();
	}

	return std::move(_unit);
}

void Parser::parseDeclaration() {
	if (at(TokenKind::Semicolon)) {
		advance();
		return;
	}

	if (matchIdentifier("using")) {
		// using namespace metal;
		advance(); // namespace
		advance(); // name
		expect(TokenKind::Semicolon, "after using directive");
		return;
	}

	if (matchIdentifier("struct")) {
		StructDecl decl = parseStructDeclaration();
		declareName(decl.name, "struct");
		_unit.structs.push_back(std::move(decl));
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

	// A file-scope constant. An address space or a type qualifier ahead of the
	// type is what marks one, since a bare "float kX" at file scope is not valid
	// MSL and would otherwise be read as the start of a function's return type.
	if (at(TokenKind::Identifier)
		&& (isTypeQualifier(current().text) || addressSpaceFor(current().text))) {
		_unit.globals.push_back(parseGlobalDeclaration());
		return;
	}

	throw CompileError("unexpected \"" + std::string(current().text) + "\" at top level; "
		"expected a struct, kernel, vertex or fragment declaration");
}

VariableDeclaration Parser::parseGlobalDeclaration() {
	VariableDeclaration declaration;

	declaration.type = parseType();

	if (declaration.type.addressSpace != AddressSpace::Constant) {
		throw CompileError("only a constant can be declared at file scope, and a "
			+ std::string(addressSpaceName(declaration.type.addressSpace))
			+ (declaration.type.isPointer ? " pointer" : " value") + " is not one");
	}

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
	}

	declaration.name = std::string(advance().text);
	declareName(declaration.name, "variable");

	if (!match(TokenKind::Assign)) {
		throw CompileError("a constant declared at file scope needs an initialiser, found "
			+ std::string(tokenKindName(kind())));
	}

	declaration.initializer = parseInitializer();

	expect(TokenKind::Semicolon, "after a file-scope declaration");
	return declaration;
}

// An initialiser, which is an expression or a braced list. A list is only
// spelled in an initialiser, so it is not a primary expression.
ExpressionPtr Parser::parseInitializer() {
	if (at(TokenKind::LBrace)) {
		return parseInitializerList();
	}

	return parseExpression();
}

ExpressionPtr Parser::parseInitializerList() {
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
	return list;
}

StructDecl Parser::parseStructDeclaration() {
	StructDecl decl;

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a struct name, found " + std::string(tokenKindName(kind())));
	}

	decl.name = std::string(advance().text);
	parseStructBody(decl);
	match(TokenKind::Semicolon);

	return decl;
}

// From the base list to the closing brace. A typedef reads the declarator after
// that brace, so the semicolon is the caller's.
void Parser::parseStructBody(StructDecl& decl) {
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
		StructField field;
		field.type = parseType();
		expectFieldName(field.name);

		// An attribute list and an array suffix both open with a bracket, but an
		// attribute list opens with two, which is what tells them apart.
		if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
			field.attributes = parseFieldAttributes();
		}

		match(TokenKind::Semicolon);
		decl.fields.push_back(std::move(field));
	}

	expect(TokenKind::RBrace, "to close a struct body");
}

namespace {

	bool sameType(const Type& a, const Type& b) {
		return a.scalar == b.scalar && a.vectorWidth == b.vectorWidth
			&& a.matrixColumns == b.matrixColumns && a.isPacked == b.isPacked
			&& a.namedType == b.namedType && a.isConst == b.isConst;
	}

	// Names the typedef and enum support has to keep distinct from everything else.
	constexpr char kAnonymousStructTypedef[] = "typedef of an anonymous struct";

	bool isTypeSupportKind(std::string_view what) {
		return what == "typedef" || what == "enum" || what == "enum constant"
			|| what == kAnonymousStructTypedef;
	}

}

bool Parser::resolveTypeName(std::string_view text, Type& out) const {
	if (isTypeName(text, out) || isSimdVectorName(text, out)) {
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

	const auto alias = _typedefs.find(std::string(text));
	if (alias == _typedefs.end()) {
		return false;
	}

	out.scalar = alias->second.scalar;
	out.vectorWidth = alias->second.vectorWidth;
	out.matrixColumns = alias->second.matrixColumns;
	out.isPacked = alias->second.isPacked;
	out.namedType = alias->second.namedType;
	out.isConst = out.isConst || alias->second.isConst;
	return true;
}

void Parser::declareName(const std::string& name, const char* what) {
	// The simd names are global typedefs in Apple's compiler, so nothing at file
	// scope can reuse one. The builtin MSL names are not: a struct may be called
	// float2, and a typedef or an enumerator may not.
	Type builtin;
	if (isSimdVectorName(name, builtin) || (isTypeSupportKind(what) && isTypeName(name, builtin))) {
		throw CompileError("\"" + name + "\" is a builtin type name and cannot be redeclared as a "
			+ what);
	}

	// Functions overload, and a struct or enum tag may share a name with a function
	// or a variable; every other pair of declarations of one name is a redefinition.
	const auto [existing, inserted] = _declared.emplace(name, what);
	const auto isTag = [](std::string_view kindName) { return kindName == "struct" || kindName == "enum"; };
	const auto isObject = [](std::string_view kindName) { return kindName == "function" || kindName == "variable"; };
	const std::string_view now(what);
	const bool overload = now == "function" && existing->second == "function";
	const bool tagBesideObject = (isTag(now) && isObject(existing->second))
		|| (isObject(now) && isTag(existing->second));
	if (!inserted && !overload && !tagBesideObject) {
		throw CompileError("redefinition of \"" + name + "\": declared as " + existing->second
			+ " and again as " + what);
	}
}

void Parser::rejectShadowing(const std::string& name) const {
	const char* what = _enumConstants.count(name) ? "enum constant"
		: _typedefs.count(name) ? "typedef"
		: _enumTypes.count(name) ? "enum type"
		: nullptr;

	if (what) {
		throw CompileError("a variable named \"" + name + "\" would hide the " + std::string(what) + " of "
			"that name, which mslc does not support");
	}
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

	const std::string name(advance().text);

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
		if (anonymousStruct) {
			return;
		}
	}

	// A typedef under the name of the struct it names is the same type, but the
	// name is then a typedef as well, which is what another declaration collides with.
	if (name == target.namedType) {
		_declared[name] = "typedef";
		return;
	}

	Type existing;
	if (resolveTypeName(name, existing)) {
		if (!sameType(existing, target)) {
			throw CompileError("typedef redefinition with different types ('" + typeName(target) + "' vs '"
				+ typeName(existing) + "')");
		}
		return;
	}

	declareName(name, "typedef");
	_typedefs[name] = target;
}

std::string Parser::parseEnumDeclaration(bool& hadBody) {
	expectKeyword("enum", "to start an enum");

	if (atKeyword("class") || atKeyword("struct")) {
		throw CompileError("a scoped enum (enum " + std::string(current().text) + ") is not supported; "
			"declare an unscoped enum");
	}

	std::string tag;
	if (kind() == TokenKind::Identifier) {
		tag = std::string(advance().text);
	}

	if (at(TokenKind::Colon)) {
		throw CompileError("an enum with a fixed underlying type is not supported");
	}

	hadBody = at(TokenKind::LBrace);
	if (!hadBody) {
		return tag;
	}

	advance();
	if (!tag.empty()) {
		declareName(tag, "enum");
		_enumTypes[tag] = tag;
	}

	// Each enumerator is the previous one plus one unless it says otherwise, and
	// the first is zero. They are kept as 64-bit values and held to what an int
	// holds, so a value that wrapped could never be mistaken for a small one.
	int64_t next = 0;
	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected an enumerator name, found " + std::string(tokenKindName(kind())));
		}

		const std::string name(advance().text);
		int64_t value = next;
		if (match(TokenKind::Assign)) {
			value = evaluateConstant(*parseAssignment(), false);
		}
		if (value > INT_MAX || value < -INT_MAX) {
			throw CompileError("the enumerator \"" + name + "\" does not fit in an int");
		}

		declareName(name, "enum constant");
		_enumConstants[name] = value;
		next = value + 1;

		if (!match(TokenKind::Comma)) {
			break;
		}
	}

	expect(TokenKind::RBrace, "to close an enum body");
	return tag;
}

// Folds the integer constant expressions an enumerator, an attribute index or an
// array length may be. Every step is held to what an int holds, so a result that
// would wrap is an error here, and that is narrower than Apple's compiler, which
// widens. A `u` literal is only accepted as the whole expression, since mixing it
// with a signed operand changes the arithmetic to unsigned, which this does not
// model.
int64_t Parser::evaluateConstant(const Expression& expression, bool nested) const {
	const auto fit = [](int64_t value) {
		if (value > INT_MAX || value < -INT_MAX) {
			throw CompileError("an integer constant expression overflows an int");
		}
		return value;
	};

	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
			if (nested && (expression.intIsUnsigned || expression.intValue > INT_MAX)) {
				throw CompileError("an unsigned or wider integer inside a constant expression is not supported");
			}
			if (expression.intValue > INT64_MAX) {
				throw CompileError("an integer constant is too large");
			}
			return static_cast<int64_t>(expression.intValue);
		case ExpressionKind::Unary: {
			const int64_t operand = evaluateConstant(*expression.left, true);
			switch (expression.unaryOperator) {
				case UnaryOperator::Negate: return fit(-operand);
				case UnaryOperator::Plus: return operand;
				case UnaryOperator::Not: return operand == 0 ? 1 : 0;
				case UnaryOperator::BitNot: return ~operand;
				default: break;
			}
			break;
		}
		case ExpressionKind::Binary: {
			const int64_t left = evaluateConstant(*expression.left, true);
			const int64_t right = evaluateConstant(*expression.right, true);
			switch (expression.binaryOperator) {
				case BinaryOperator::Add: return fit(left + right);
				case BinaryOperator::Subtract: return fit(left - right);
				case BinaryOperator::Multiply: return fit(left * right);
				case BinaryOperator::Divide:
				case BinaryOperator::Modulo:
					if (right == 0) {
						throw CompileError("division by zero in a constant expression");
					}
					return expression.binaryOperator == BinaryOperator::Divide ? fit(left / right) : left % right;
				case BinaryOperator::ShiftLeft:
				case BinaryOperator::ShiftRight:
					if (right < 0 || right > 31 || left < 0) {
						throw CompileError("a shift in a constant expression has a negative operand or a "
							"count outside 0 to 31");
					}
					return expression.binaryOperator == BinaryOperator::ShiftLeft ? fit(left << right) : left >> right;
				case BinaryOperator::BitAnd: return left & right;
				case BinaryOperator::BitOr: return left | right;
				case BinaryOperator::BitXor: return left ^ right;
				case BinaryOperator::Less: return left < right;
				case BinaryOperator::LessEqual: return left <= right;
				case BinaryOperator::Greater: return left > right;
				case BinaryOperator::GreaterEqual: return left >= right;
				case BinaryOperator::Equal: return left == right;
				case BinaryOperator::NotEqual: return left != right;
				case BinaryOperator::LogicalAnd: return left != 0 && right != 0;
				case BinaryOperator::LogicalOr: return left != 0 || right != 0;
			}
			break;
		}
		default:
			break;
	}

	throw CompileError("not an integer constant expression");
}

// The index an attribute or an array length takes: a constant that is not
// negative and fits the 32 bits it is stored in.
int64_t Parser::parseConstantIndex(const std::string& context) {
	int64_t value = 0;
	try {
		value = evaluateConstant(*parseAssignment(), false);
	} catch (const CompileError& error) {
		throw CompileError(context + " needs a constant integer argument (" + error.what() + ")");
	}

	if (value < 0 || value > UINT32_MAX) {
		throw CompileError(context + " needs a constant integer argument between 0 and 4294967295, found "
			+ std::to_string(value));
	}

	return value;
}

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
static void checkAttributeIndex(const std::string& name, uint32_t index) {
	const uint32_t limit = name == "texture" ? 127 : name == "sampler" ? 15 : 30;
	if (index > limit) {
		throw CompileError("attribute \"" + name + "\" index " + std::to_string(index)
			+ " is out of bounds: it must be between 0 and " + std::to_string(limit));
	}
}

std::optional<uint32_t> Parser::tryParseArrayLength() {
	if (!at(TokenKind::LBracket)) {
		return std::nullopt;
	}

	advance();

	const auto length = static_cast<uint32_t>(parseConstantIndex("an array length"));
	expect(TokenKind::RBracket, "to close an array length");
	return length;
}

Type Parser::parseType(bool allowResource) {
	Type type;

	// MSL writes the address space and the const qualifier in either order, and
	// both orders are ordinary: "device const float*" and "const device Vertex *"
	// are the same declaration spelled two ways. Reading them as two fixed
	// sequences took "const" as the whole prefix and then read "device" as a type
	// name, so the parameter became "Vertex" and the list ended at the "*".
	while (kind() == TokenKind::Identifier) {
		const std::string_view text = current().text;

		AddressSpace space;
		if (parseAddressSpace(space)) {
			// A second address space is a mistake rather than an alternative
			// spelling, and which one was meant decides which storage class a
			// binding lands in, so it is reported rather than resolved.
			if (type.addressSpace != AddressSpace::None) {
				throw CompileError("a type has one address space, found \"" + std::string(text)
					+ "\" after another");
			}

			type.addressSpace = space;
			continue;
		}

		if (!isTypeQualifier(text)) {
			break;
		}

		if (text == "const") {
			type.isConst = true;
		}

		advance();
	}

	// metal::sampler and metal::texture2d, which Apple takes as the unqualified names.
	if (kind() == TokenKind::Identifier && current().text == "metal" && lookahead().kind == TokenKind::ColonColon
		&& lookahead(2).kind == TokenKind::Identifier && isResourceTypeName(lookahead(2).text)) {
		advance();
		advance();
	}

	if (kind() == TokenKind::Identifier && isResourceTypeName(current().text)) {
		if (!allowResource) {
			throw CompileError("\"" + std::string(current().text) + "\" is a texture or sampler type, "
				"which mslc takes as an entry point parameter only, and a sampler also as a local");
		}
		parseResourceType(type);
		if (atKeyword("const")) {
			type.isConst = true;
			advance();
		}
	} else if (kind() == TokenKind::Identifier && resolveTypeName(current().text, type)) {
		advance();
	} else if (kind() == TokenKind::Identifier && _enumTypes.count(std::string(current().text))) {
		throw CompileError("the enum type \"" + std::string(current().text) + "\" is not supported as a "
			"type; its enumerators are, as integer constants");
	} else if (atKeyword("enum")) {
		throw CompileError("an enum used as a type is not supported; its enumerators are, as integer constants");
	} else if (kind() == TokenKind::Identifier) {
		type.namedType = std::string(advance().text);
	} else {
		throw CompileError("expected a type, found " + std::string(tokenKindName(kind()))
			+ " \"" + std::string(current().text) + "\"");
	}

	while (at(TokenKind::Star)) {
		if (type.isPointer) {
			throw CompileError("a pointer to a pointer is not supported");
		}
		advance();
		type.isPointer = true;
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

	if (name != "texture2d") {
		throw CompileError("\"" + name + "\" is not lowered yet; mslc lowers texture2d<float>, "
			"texture2d<half> and sampler");
	}

	expect(TokenKind::Less, "to open the sampled type of texture2d");
	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected the sampled type of texture2d, found "
			+ std::string(tokenKindName(kind())));
	}

	const std::string component(advance().text);
	if (component != "float" && component != "half") {
		throw CompileError("texture2d<" + component + "> is not lowered yet; mslc lowers the "
			"sampled types float and half");
	}

	if (match(TokenKind::Comma)) {
		if (kind() == TokenKind::Identifier && current().text == "metal" && lookahead().kind == TokenKind::ColonColon) {
			advance();
			advance();
		}
		if (kind() != TokenKind::Identifier || current().text != "access") {
			throw CompileError("expected an access qualifier after the sampled type of texture2d");
		}
		advance();
		expect(TokenKind::ColonColon, "after \"access\"");
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected an access qualifier name after \"access::\"");
		}
		const std::string access(advance().text);
		if (access != "sample") {
			throw CompileError("texture2d access::" + access + " is not lowered yet; mslc lowers "
				"access::sample, which is the default");
		}
	}

	expect(TokenKind::Greater, "to close the sampled type of texture2d");
	type.resource = ResourceKind::Texture2D;
	type.scalar = component == "half" ? ScalarKind::Half : ScalarKind::Float;
}

Parameter Parser::parseParameter() {
	Parameter param;

	param.type = parseType(true);

	// "constant BufferClearParams &params" and "constant BufferClearParams
	// *params" name the same buffer and lower to the same descriptor, so a
	// reference is consumed and nothing is recorded for it.
	if (at(TokenKind::Ampersand) && param.type.resource != ResourceKind::None) {
		throw CompileError("a reference to " + typeName(param.type) + " is not valid; a texture or "
			"sampler parameter is taken by value");
	}
	match(TokenKind::Ampersand);

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a parameter name, found " + std::string(tokenKindName(kind())));
	}

	param.name = std::string(advance().text);
	rejectShadowing(param.name);

	if (param.type.resource != ResourceKind::None
		&& (param.type.isPointer || param.type.arrayLength || param.type.addressSpace != AddressSpace::None)) {
		throw CompileError("parameter \"" + param.name + "\" is a pointer to or an array of "
			+ typeName(param.type) + " or has an address space, which is not lowered yet; mslc "
			"takes a texture or sampler by value");
	}

	// "float values[4]" is an array suffix, "[[buffer(0)]]" is an attribute
	// list, and both start with a bracket, so the second one has to be
	// checked before the first is consumed.
	if (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
		advance();
		expect(TokenKind::RBracket, "to close an array parameter");
	}

	if (at(TokenKind::LBracket)) {
		param.attributes = parseParameterAttributes();
	}

	return param;
}

ParameterAttributes Parser::parseParameterAttributes() {
	ParameterAttributes attributes;

	parseAttributeList([&](const std::string& name, std::optional<uint32_t> argument) {
		if (argument && (name == "buffer" || name == "texture" || name == "sampler")) {
			checkAttributeIndex(name, *argument);
		}

		if (name == "buffer") {
			if (!argument) {
				throw CompileError("[[buffer]] needs an index");
			}
			attributes.bufferIndex = argument;
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
		} else if (name == "stage_in") {
			attributes.stageIn = true;
		} else if (auto builtin = builtinFromName(name)) {
			attributes.builtin = builtin;
		} else if (isMSLBuiltinName(name)) {
			throw CompileError("builtin attribute \"" + name + "\" is recognised but not "
				"supported yet; mslc reports it rather than ignoring it");
		} else {
			throw CompileError("unsupported attribute \"" + name + "\"");
		}
	});

	return attributes;
}

FieldAttributes Parser::parseFieldAttributes() {
	FieldAttributes attributes;

	parseAttributeList([&](const std::string& name, std::optional<uint32_t> argument) {
		if (name == "position") {
			attributes.position = true;
		} else if (name == "attribute") {
			if (!argument) {
				throw CompileError("[[attribute]] needs an index");
			}
			checkAttributeIndex(name, *argument);
			attributes.attributeIndex = argument;
		} else if (builtinFromName(name)) {
			throw CompileError("builtin attribute \"" + name + "\" is not valid on a struct "
				"field; only [[position]] and [[attribute(n)]] are");
		} else {
			throw CompileError("unsupported attribute \"" + name + "\" on a struct field");
		}
	});

	return attributes;
}

void Parser::parseAttributeList(const std::function<void(const std::string&, std::optional<uint32_t>)>& visit) {
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

		std::optional<uint32_t> argument;
		if (at(TokenKind::LParen)) {
			advance();
			argument = static_cast<uint32_t>(parseConstantIndex("attribute \"" + name + "\""));
			expect(TokenKind::RParen, "to close an attribute argument");
		}

		visit(name, argument);

		match(TokenKind::Comma);
	}

	expect(TokenKind::RBracket, "to close an attribute list");
}

FunctionDecl Parser::parseFunctionDeclaration(Stage stage) {
	FunctionDecl decl;
	decl.stage = stage;
	decl.line = line();

	decl.returnType = parseType();

	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a function name, found " + std::string(tokenKindName(kind())));
	}

	decl.name = std::string(advance().text);
	declareName(decl.name, "function");

	expect(TokenKind::LParen, "to open a parameter list");

	while (!at(TokenKind::RParen) && !at(TokenKind::EndOfFile)) {
		decl.parameters.push_back(parseParameter());
		if (!match(TokenKind::Comma)) {
			break;
		}
	}

	expect(TokenKind::RParen, "to close a parameter list");

	// Two parameters of one kind cannot take the same slot.
	std::set<std::pair<std::string, uint32_t>> slots;
	for (const Parameter& parameter: decl.parameters) {
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

	// parseCompoundStatement consumes the opening brace itself, so consuming it
	// here as well would make it look for a second one.
	decl.body = parseCompoundStatement();

	return decl;
}

StatementPtr Parser::parseCompoundStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Compound;
	statement->line = line();

	expect(TokenKind::LBrace, "to open a block");

	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		statement->children.push_back(parseStatement());
	}

	expect(TokenKind::RBrace, "to close a block");

	return statement;
}

StatementPtr Parser::parseStatement() {
	auto statement = std::make_unique<Statement>();
	statement->line = line();

	if (at(TokenKind::LBrace)) {
		return parseCompoundStatement();
	}

	if (at(TokenKind::Semicolon)) {
		advance();
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
	}

	if (atKeyword("typedef") || atKeyword("enum")) {
		throw CompileError("\"" + std::string(current().text) + "\" inside a function is not supported; "
			"declare it at file scope");
	}

	if (atKeyword("if")) {
		return parseIfStatement();
	}

	if (atKeyword("for")) {
		return parseForStatement();
	}

	if (atKeyword("while")) {
		return parseWhileStatement();
	}

	if (atKeyword("return")) {
		return parseReturnStatement();
	}

	if (atKeyword("break") || atKeyword("continue") || atKeyword("discard_fragment")) {
		const std::string keyword(advance().text);
		statement->kind = keyword == "break" ? StatementKind::Break
			: keyword == "continue" ? StatementKind::Continue
			: StatementKind::Discard;
		expect(TokenKind::Semicolon, "after a jump statement");
		return statement;
	}

	// A declaration statement starts with a type. Distinguish it from an
	// expression by looking for a bare type name, a type qualifier or a known
	// address space before anything that could begin an expression. A struct
	// declared earlier in the unit counts, since "Vertex vtx;" is a declaration
	// and nothing else could start with those two words.
	{
		Type probe;
		const bool looksLikeType =
			(kind() == TokenKind::Identifier && (resolveTypeName(current().text, probe)
				|| isTypeQualifier(current().text) || isResourceTypeName(current().text)
				|| _enumTypes.count(std::string(current().text)) > 0
				|| _unit.findStruct(std::string(current().text)) != nullptr))
			|| atKeyword("device") || atKeyword("constant")
			|| atKeyword("threadgroup") || atKeyword("thread");

		if (looksLikeType) {
			statement->kind = StatementKind::DeclarationStatement;
			VariableDeclaration declaration;
			const size_t typeStart = _position;
			declaration.type = parseType(true);
			if (declaration.type.resource != ResourceKind::None) {
				for (size_t at = typeStart; at < _position; ++at) {
					if (_tokens[at].text == "static") {
						throw CompileError("variables in function scope cannot be declared static");
					}
				}
			}

			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
			}
			declaration.name = std::string(advance().text);
			rejectShadowing(declaration.name);

			if (declaration.type.resource != ResourceKind::None) {
				parseSamplerLocal(declaration);
				statement->declaration = std::move(declaration);
				return statement;
			}

			if (declaration.type.isConst && !declaration.type.isPointer && !at(TokenKind::Assign)
				&& !at(TokenKind::LParen) && !at(TokenKind::LBracket) && !at(TokenKind::LBrace)) {
				throw CompileError("the const variable \"" + declaration.name + "\" needs an initialiser");
			}

			if (match(TokenKind::Assign)) {
				declaration.initializer = parseExpression();
			} else if (at(TokenKind::LParen)) {
				// Direct initialisation, float3 specularTerm(0); . The name comes
				// first here, so the list after it constructs the variable's own
				// type rather than calling it, and the two spellings are the same
				// value. Metal has no other form: "float3(0) specularTerm;" is a
				// parse error there, which xcrun metal confirms.
				advance();
				auto construct = std::make_unique<Expression>();
				construct->kind = ExpressionKind::Construct;
				construct->line = line();
				construct->constructType = declaration.type;
				construct->arguments = parseArgumentList("to close a constructor's argument list");
				declaration.initializer = std::move(construct);
			}

			expect(TokenKind::Semicolon, "after a declaration");
			statement->declaration = std::move(declaration);
			return statement;
		}
	}

	statement->kind = StatementKind::ExpressionStatement;
	statement->expression = parseExpression();
	expect(TokenKind::Semicolon, "after an expression statement");

	return statement;
}

// A sampler declared in the shader: "constexpr sampler s(options);", with a
// brace list or "= sampler(options)" for the options, or with none. Apple's
// compiler takes the same declaration without constexpr, so this does too.
void Parser::parseSamplerLocal(VariableDeclaration& declaration) {
	const Type& type = declaration.type;
	if (type.resource != ResourceKind::Sampler) {
		throw CompileError("a local \"" + declaration.name + "\" of type " + typeName(type)
			+ " is not lowered yet; a texture is an entry point parameter");
	}
	if (type.isPointer || type.arrayLength
		|| (type.addressSpace != AddressSpace::None && type.addressSpace != AddressSpace::Thread)) {
		throw CompileError("a local sampler \"" + declaration.name + "\" with a pointer, an array "
			"or an address space other than thread is not lowered yet");
	}

	if (match(TokenKind::LParen)) {
		if (at(TokenKind::RParen)) {
			throw CompileError("\"" + declaration.name + "\" with empty parentheses declares a function, "
				"not a sampler; write the sampler with no parentheses or with braces");
		}
		declaration.sampler = parseSamplerOptions(TokenKind::RParen);
	} else if (match(TokenKind::LBrace)) {
		declaration.sampler = parseSamplerOptions(TokenKind::RBrace);
	} else if (match(TokenKind::Assign)) {
		if (match(TokenKind::LBrace)) {
			declaration.sampler = parseSamplerOptions(TokenKind::RBrace);
			expect(TokenKind::Semicolon, "after a declaration");
			return;
		}
		if (kind() != TokenKind::Identifier || current().text != "sampler"
			|| lookahead().kind != TokenKind::LParen) {
			throw CompileError("a sampler local \"" + declaration.name + "\" is initialised from "
				"\"sampler(options)\" only; a copy of another sampler is not lowered yet");
		}
		advance();
		advance();
		declaration.sampler = parseSamplerOptions(TokenKind::RParen);
	} else {
		declaration.sampler = SamplerState();
	}

	expect(TokenKind::Semicolon, "after a declaration");
}

// The options of a constexpr sampler, "mag_filter::linear, address::repeat", up
// to and including the closing token. The restrictions are Apple's: pixel
// coordinates allow only equal filters, no mip filter and the clamp address modes.
SamplerState Parser::parseSamplerOptions(TokenKind closing) {
	SamplerState state;
	std::set<std::string> assigned;

	while (!at(closing) && !at(TokenKind::EndOfFile)) {
		if (kind() == TokenKind::Identifier && current().text == "metal" && lookahead().kind == TokenKind::ColonColon) {
			advance();
			advance();
		}
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected a sampler option, found " + std::string(tokenKindName(kind())));
		}

		const std::string family(advance().text);
		static const std::set<std::string> unlowered = {
			"compare_func", "max_anisotropy", "lod_clamp", "border_color", "reduction",
		};
		static const std::set<std::string> known = {
			"coord", "address", "s_address", "t_address", "r_address", "filter",
			"mag_filter", "min_filter", "mip_filter",
		};
		if (unlowered.count(family)) {
			throw CompileError("the sampler option \"" + family + "\" is not lowered yet; mslc lowers "
				"coord, address, s_address, t_address, r_address, filter, mag_filter, min_filter and "
				"mip_filter");
		}
		if (!known.count(family)) {
			throw CompileError("\"" + family + "\" is not a sampler option");
		}

		expect(TokenKind::ColonColon, "after a sampler option name");
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected a value after \"" + family + "::\"");
		}
		const std::string value(advance().text);
		const std::string spelled = family + "::" + value;

		const auto address = [&]() -> SamplerAddress {
			if (value == "clamp_to_zero" || value == "clamp_to_border") return SamplerAddress::ClampToZero;
			if (value == "clamp_to_edge") return SamplerAddress::ClampToEdge;
			if (value == "repeat") return SamplerAddress::Repeat;
			if (value == "mirrored_repeat") return SamplerAddress::MirroredRepeat;
			throw CompileError("\"" + spelled + "\" is not a sampler address mode");
		};
		const auto filter = [&]() -> SamplerFilter {
			if (value == "nearest") return SamplerFilter::Nearest;
			if (value == "linear") return SamplerFilter::Linear;
			throw CompileError("\"" + spelled + "\" is not a sampler filter");
		};

		// Apple keeps the first option that names an attribute and ignores a later
		// one for it: "address::repeat, address::clamp_to_edge" is repeat, and
		// "s_address::repeat, address::clamp_to_edge" repeats s and clamps t and r.
		// So an option only fills the attributes nothing before it set.
		const auto fill = [&](std::initializer_list<const char*> attributes, auto&& assign) {
			for (const char* attribute: attributes) {
				if (assigned.insert(attribute).second) {
					assign(attribute);
				}
			}
		};

		if (family == "coord") {
			if (value != "normalized" && value != "pixel") {
				throw CompileError("\"" + spelled + "\" is not a coordinate mode");
			}
			fill({ "coord" }, [&](const char*) { state.normalizedCoordinates = value != "pixel"; });
		} else if (family == "address" || family == "s_address" || family == "t_address"
			|| family == "r_address") {
			const SamplerAddress mode = address();
			fill(family == "address" ? std::initializer_list<const char*>{ "s", "t", "r" }
					: family == "s_address" ? std::initializer_list<const char*>{ "s" }
					: family == "t_address" ? std::initializer_list<const char*>{ "t" }
					: std::initializer_list<const char*>{ "r" },
				[&](const char* attribute) {
					(attribute[0] == 's' ? state.sAddress : attribute[0] == 't' ? state.tAddress
						: state.rAddress) = mode;
				});
		} else if (family == "filter" || family == "mag_filter" || family == "min_filter") {
			const SamplerFilter chosen = filter();
			fill(family == "filter" ? std::initializer_list<const char*>{ "mag", "min" }
					: family == "mag_filter" ? std::initializer_list<const char*>{ "mag" }
					: std::initializer_list<const char*>{ "min" },
				[&](const char* attribute) {
					(attribute[1] == 'a' ? state.magFilter : state.minFilter) = chosen;
				});
		} else {
			SamplerMipFilter chosen = SamplerMipFilter::None;
			if (value == "none") chosen = SamplerMipFilter::None;
			else if (value == "nearest") chosen = SamplerMipFilter::Nearest;
			else if (value == "linear") chosen = SamplerMipFilter::Linear;
			else throw CompileError("\"" + spelled + "\" is not a mip filter");
			fill({ "mip" }, [&](const char*) { state.mipFilter = chosen; });
		}

		if (!match(TokenKind::Comma)) {
			break;
		}
		if (at(closing)) {
			throw CompileError("expected a sampler option after ',' in a sampler's option list");
		}
	}

	expect(closing, "to close a sampler's option list");

	if (!state.normalizedCoordinates) {
		const auto clamps = [](SamplerAddress mode) {
			return mode == SamplerAddress::ClampToZero || mode == SamplerAddress::ClampToEdge;
		};
		if (state.magFilter != state.minFilter || state.mipFilter != SamplerMipFilter::None
			|| !clamps(state.sAddress) || !clamps(state.tAddress) || !clamps(state.rAddress)) {
			throw CompileError("invalid values combination in sampler initialization: with "
				"coord::pixel the min_filter and mag_filter have to be the same, the mip_filter "
				"none, and the address modes clamp_to_zero, clamp_to_edge or clamp_to_border");
		}
	}

	return state;
}

StatementPtr Parser::parseIfStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::If;
	statement->line = line();

	expectKeyword("if", "at the start of an if");
	expect(TokenKind::LParen, "after 'if'");
	statement->expression = parseExpression();
	expect(TokenKind::RParen, "after an if condition");

	statement->thenBranch = parseStatement();

	if (matchIdentifier("else")) {
		statement->elseBranch = parseStatement();
	}

	return statement;
}

StatementPtr Parser::parseForStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::For;
	statement->line = line();

	expectKeyword("for", "at the start of a for");
	expect(TokenKind::LParen, "after 'for'");

	{
		Type probe;
		const bool looksLikeType =
			kind() == TokenKind::Identifier && (resolveTypeName(current().text, probe)
				|| _enumTypes.count(std::string(current().text)) > 0);

		if (looksLikeType) {
			VariableDeclaration declaration;
			declaration.type = parseType();
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a loop variable name in a for initialiser");
			}
			declaration.name = std::string(advance().text);
			rejectShadowing(declaration.name);
			if (match(TokenKind::Assign)) {
				declaration.initializer = parseExpression();
			}
			statement->forInitializer = std::move(declaration);
		} else {
			statement->expression = parseExpression();
		}
	}

	expect(TokenKind::Semicolon, "after a for initialiser");

	if (!at(TokenKind::Semicolon)) {
		statement->forCondition = parseExpression();
	}
	expect(TokenKind::Semicolon, "after a for condition");

	if (!at(TokenKind::RParen)) {
		statement->forIncrement = parseExpression();
	}
	expect(TokenKind::RParen, "after a for header");

	statement->forBody = parseStatement();

	return statement;
}

StatementPtr Parser::parseWhileStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::While;
	statement->line = line();

	expectKeyword("while", "at the start of a while");
	expect(TokenKind::LParen, "after 'while'");
	statement->whileCondition = parseExpression();
	expect(TokenKind::RParen, "after a while condition");

	statement->whileBody = parseStatement();

	return statement;
}

StatementPtr Parser::parseReturnStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Return;
	statement->line = line();

	expectKeyword("return", "at the start of a return");

	if (!at(TokenKind::Semicolon)) {
		statement->expression = parseExpression();
	}

	expect(TokenKind::Semicolon, "after a return");

	return statement;
}

ExpressionPtr Parser::parseExpression() {
	return parseAssignment();
}

ExpressionPtr Parser::parseAssignment() {
	auto left = parseLogicalOr();

	static const std::pair<TokenKind, BinaryOperator> compounds[] = {
		{ TokenKind::PlusAssign, BinaryOperator::Add },
		{ TokenKind::MinusAssign, BinaryOperator::Subtract },
		{ TokenKind::StarAssign, BinaryOperator::Multiply },
		{ TokenKind::SlashAssign, BinaryOperator::Divide },
		{ TokenKind::PercentAssign, BinaryOperator::Modulo },
		{ TokenKind::AmpersandAssign, BinaryOperator::BitAnd },
		{ TokenKind::PipeAssign, BinaryOperator::BitOr },
		{ TokenKind::CaretAssign, BinaryOperator::BitXor },
		{ TokenKind::ShiftLeftAssign, BinaryOperator::ShiftLeft },
		{ TokenKind::ShiftRightAssign, BinaryOperator::ShiftRight },
	};

	std::optional<BinaryOperator> compound;
	for (const auto& [token, op]: compounds) {
		if (at(token)) {
			compound = op;
		}
	}

	if (at(TokenKind::Assign) || compound) {
		advance();
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Assign;
		expression->compoundOperator = compound;
		expression->line = left->line;
		expression->left = std::move(left);
		expression->right = parseAssignment();
		return expression;
	}

	return left;
}











ExpressionPtr Parser::parseLogicalOr() {
	return parseBinaryLevel([](Parser& p) { return p.parseLogicalAnd(); }, { { TokenKind::OrOr, BinaryOperator::LogicalOr } });
}

ExpressionPtr Parser::parseLogicalAnd() {
	return parseBinaryLevel([](Parser& p) { return p.parseBitwiseOr(); }, { { TokenKind::AndAnd, BinaryOperator::LogicalAnd } });
}

ExpressionPtr Parser::parseBitwiseOr() {
	return parseBinaryLevel([](Parser& p) { return p.parseBitwiseXor(); }, { { TokenKind::Pipe, BinaryOperator::BitOr } });
}

ExpressionPtr Parser::parseBitwiseXor() {
	return parseBinaryLevel([](Parser& p) { return p.parseBitwiseAnd(); }, { { TokenKind::Caret, BinaryOperator::BitXor } });
}

ExpressionPtr Parser::parseBitwiseAnd() {
	return parseBinaryLevel([](Parser& p) { return p.parseEquality(); }, { { TokenKind::Ampersand, BinaryOperator::BitAnd } });
}

ExpressionPtr Parser::parseEquality() {
	return parseBinaryLevel([](Parser& p) { return p.parseRelational(); }, { { TokenKind::Equal, BinaryOperator::Equal }, { TokenKind::NotEqual, BinaryOperator::NotEqual } });
}

ExpressionPtr Parser::parseRelational() {
	return parseBinaryLevel([](Parser& p) { return p.parseShift(); }, { { TokenKind::Less, BinaryOperator::Less }, { TokenKind::LessEqual, BinaryOperator::LessEqual },
		  { TokenKind::Greater, BinaryOperator::Greater }, { TokenKind::GreaterEqual, BinaryOperator::GreaterEqual } });
}

ExpressionPtr Parser::parseShift() {
	return parseBinaryLevel([](Parser& p) { return p.parseAdditive(); }, { { TokenKind::ShiftLeft, BinaryOperator::ShiftLeft }, { TokenKind::ShiftRight, BinaryOperator::ShiftRight } });
}

ExpressionPtr Parser::parseAdditive() {
	return parseBinaryLevel([](Parser& p) { return p.parseMultiplicative(); }, { { TokenKind::Plus, BinaryOperator::Add }, { TokenKind::Minus, BinaryOperator::Subtract } });
}

ExpressionPtr Parser::parseMultiplicative() {
	return parseBinaryLevel([](Parser& p) { return p.parseUnary(); }, { { TokenKind::Star, BinaryOperator::Multiply }, { TokenKind::Slash, BinaryOperator::Divide },
		  { TokenKind::Percent, BinaryOperator::Modulo } });
}

ExpressionPtr Parser::parseUnary() {
	// Only in prefix position: a '*' between two operands never gets here, because
	// parseMultiplicative consumes it after the left operand is complete.
	if (at(TokenKind::Star)) {
		auto dereference = std::make_unique<Expression>();
		dereference->kind = ExpressionKind::Index;
		dereference->isDereference = true;
		dereference->line = line();
		advance();

		auto operand = parseUnary();
		if (operand->kind == ExpressionKind::Binary) {
			throw CompileError("pointer arithmetic such as \"*(p + i)\" is not supported; "
				"index the pointer instead, \"p[i]\"");
		}
		if (operand->isDereference) {
			throw CompileError("dereferencing a pointer to a pointer is not supported");
		}

		auto zero = std::make_unique<Expression>();
		zero->kind = ExpressionKind::IntLiteral;
		zero->line = dereference->line;

		dereference->left = std::move(operand);
		dereference->arguments.push_back(std::move(zero));
		return dereference;
	}

	if (at(TokenKind::Ampersand)) {
		throw CompileError("taking an address with '&' is not supported");
	}

	if (at(TokenKind::Minus) || at(TokenKind::Plus) || at(TokenKind::Bang) || at(TokenKind::Tilde)
		|| at(TokenKind::Increment) || at(TokenKind::Decrement)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Unary;
		expression->line = line();

		switch (kind()) {
			case TokenKind::Minus: expression->unaryOperator = UnaryOperator::Negate; break;
			case TokenKind::Plus: expression->unaryOperator = UnaryOperator::Plus; break;
			case TokenKind::Bang: expression->unaryOperator = UnaryOperator::Not; break;
			case TokenKind::Tilde: expression->unaryOperator = UnaryOperator::BitNot; break;
			case TokenKind::Increment: expression->unaryOperator = UnaryOperator::PreIncrement; break;
			default: expression->unaryOperator = UnaryOperator::PreDecrement; break;
		}

		advance();
		expression->left = parseUnary();
		return expression;
	}

	return parsePostfix();
}

ExpressionPtr Parser::parsePostfix() {
	auto expression = parsePrimary();

	while (true) {
		if (at(TokenKind::LBracket)) {
			advance();
			auto index = std::make_unique<Expression>();
			index->kind = ExpressionKind::Index;
			index->line = expression->line;
			index->left = std::move(expression);
			index->arguments.push_back(parseExpression());
			expect(TokenKind::RBracket, "to close an index");
			expression = std::move(index);
			continue;
		}

		// "->" reaches a member of what a pointer points at, which for the
		// emitter is the same member access the pointer itself would give.
		if (at(TokenKind::Dot) || at(TokenKind::Arrow)) {
			const std::string op(advance().text);
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a member name after '" + op + "'");
			}
			auto member = std::make_unique<Expression>();
			member->kind = ExpressionKind::Member;
			member->line = expression->line;
			member->left = std::move(expression);
			member->memberName = std::string(advance().text);
			expression = std::move(member);
			continue;
		}

		if (at(TokenKind::LParen)) {
			advance();
			auto call = std::make_unique<Expression>();
			call->kind = ExpressionKind::Call;
			call->line = expression->line;
			call->left = std::move(expression);
			call->arguments = parseArgumentList("to close an argument list");
			expression = std::move(call);
			continue;
		}

		return expression;
	}
}

std::vector<ExpressionPtr> Parser::parseArgumentList(const char* closing) {
	std::vector<ExpressionPtr> arguments;

	while (!at(TokenKind::RParen) && !at(TokenKind::EndOfFile)) {
		arguments.push_back(parseExpression());

		if (!match(TokenKind::Comma)) {
			break;
		}

		// A trailing comma is not a list with an empty last element; Apple's
		// compiler reports "expected expression" for it. Accepting it here would
		// mean mslc compiles a source the GPU compiler rejects.
		if (at(TokenKind::RParen)) {
			throw CompileError("expected an expression after ',' in an argument list");
		}
	}

	expect(TokenKind::RParen, closing);
	return arguments;
}

ExpressionPtr Parser::parsePrimary() {
	if (at(TokenKind::IntegerLiteral)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::IntLiteral;
		expression->line = line();
		const Token literal = advance();
		expression->intValue = literal.integerValue;
		expression->intIsUnsigned = literal.integerIsUnsigned;
		return expression;
	}

	if (at(TokenKind::FloatLiteral)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::FloatLiteral;
		expression->line = line();
		expression->floatValue = advance().floatValue;
		return expression;
	}

	if (atKeyword("true") || atKeyword("false")) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::BoolLiteral;
		expression->line = line();
		expression->boolValue = atKeyword("true");
		advance();
		return expression;
	}

	if (at(TokenKind::LParen)) {
		advance();
		auto expression = parseExpression();
		expect(TokenKind::RParen, "to close a parenthesised expression");
		return expression;
	}

	if (kind() == TokenKind::Identifier) {
		std::string_view text = current().text;

		Type type;
		if (resolveTypeName(text, type) && type.namedType.empty()) {
			// A type name in expression position constructs a value: float(x),
			// float3(0), float4(a, b, c, 1). Metal has no cast syntax, so T(...) is
			// always a constructor call, and the parenthesised part is a list of
			// arguments rather than the single operand a cast would take.
			advance();

			if (!at(TokenKind::LParen)) {
				throw CompileError("expected '(' after type \"" + std::string(text) + "\"");
			}

			advance();
			auto expression = std::make_unique<Expression>();
			expression->kind = ExpressionKind::Construct;
			expression->line = line();
			expression->constructType = type;
			expression->arguments = parseArgumentList("to close a constructor's argument list");
			return expression;
		}

		// An enumerator is the integer it was declared with.
		const auto constant = _enumConstants.find(std::string(text));
		if (constant != _enumConstants.end()) {
			auto expression = std::make_unique<Expression>();
			expression->line = line();
			advance();
			expression->kind = ExpressionKind::IntLiteral;
			expression->intValue = static_cast<uint64_t>(constant->second < 0 ? -constant->second : constant->second);
			if (constant->second >= 0) {
				return expression;
			}

			auto negated = std::make_unique<Expression>();
			negated->kind = ExpressionKind::Unary;
			negated->line = expression->line;
			negated->unaryOperator = UnaryOperator::Negate;
			negated->left = std::move(expression);
			return negated;
		}

		// A name that is also an MSL builtin is a plain identifier here. Whether
		// it is the builtin or a parameter that shadows one is a question about
		// scope, which the parser has no answer for, so the emitter decides.
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Identifier;
		expression->line = line();
		expression->name = std::string(advance().text);
		return expression;
	}

	throw CompileError("unexpected " + std::string(tokenKindName(kind())) + " \""
		+ std::string(current().text) + "\" in an expression");
}

}
