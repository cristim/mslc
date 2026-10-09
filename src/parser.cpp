#include "parser.h"

#include <algorithm>
#include <climits>
#include <map>
#include <unordered_map>

namespace mslc {

namespace {
	constexpr const char* kStdlibNamespace = "metal";
	constexpr const char* kAnonymousNamespace = "(anonymous namespace)";

	std::string joinParts(const std::vector<std::string>& parts, size_t from, size_t to) {
		std::string out;
		for (size_t i = from; i < to; ++i) {
			out += (out.empty() ? "" : "::") + parts[i];
		}
		return out;
	}


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

	// The words C++ combines, in any order, into one integer type name:
	// "unsigned long int" is a ulong, and so is "int long unsigned".
	bool isIntegerSpecifier(std::string_view text) {
		return text == "unsigned" || text == "signed" || text == "short" || text == "long"
			|| text == "int" || text == "char";
	}

	// Keywords that may appear before a type and are not address spaces.
	bool isTypeQualifier(std::string_view text) {
		return text == "const" || text == "static" || text == "constexpr"
			|| text == "volatile" || text == "__restrict";
	}

	// The texture and sampler type names. Only texture2d, texturecube and sampler are lowered;
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

	std::optional<Interpolation> interpolationFromName(std::string_view name) {
		static const std::map<std::string_view, Interpolation> table = {
			{ "flat", Interpolation::Flat },
			{ "center_perspective", Interpolation::CenterPerspective },
			{ "center_no_perspective", Interpolation::CenterNoPerspective },
			{ "centroid_perspective", Interpolation::CentroidPerspective },
			{ "centroid_no_perspective", Interpolation::CentroidNoPerspective },
			{ "sample_perspective", Interpolation::SamplePerspective },
			{ "sample_no_perspective", Interpolation::SampleNoPerspective },
		};
		const auto found = table.find(name);
		return found == table.end() ? std::nullopt : std::optional<Interpolation>(found->second);
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

	// A variable in a function lives in the thread address space, or in
	// threadgroup, and is never static; Apple reports anything else. On a
	// pointer the address space is the pointee's, which is free to be device.
	// A reference is valid MSL that mslc does not lower, so it gets its own diagnostic
	// rather than an address-space one.
	void rejectLocalQualifiers(const Type& type, bool isReference, bool referenceAllowed = false) {
		if (isReference && !referenceAllowed) {
			throw CompileError("a reference in function scope is not lowered yet");
		}
		if (type.isStatic) {
			throw CompileError("variables in function scope cannot be declared static");
		}
		// A reference names a place that is already in its own address space, so
		// "const device S &r = buffer[i];" is the one way to spell it.
		if (!isReference && !type.isPointer && (type.addressSpace == AddressSpace::Device || type.addressSpace == AddressSpace::Constant)) {
			throw CompileError(std::string("variables in function scope cannot be in the ")
				+ addressSpaceName(type.addressSpace) + " address space");
		}
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

bool Parser::isDiscardFragment(QualifiedName& name) const {
	if (!peekQualifiedName(name) || name.parts.back() != "discard_fragment"
		|| lookahead(name.tokens).kind != TokenKind::LParen
		|| lookahead(name.tokens + 1).kind != TokenKind::RParen
		|| (!name.global && name.parts.size() == 1 && isLocal("discard_fragment"))) {
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
		parseUsing();
		return;
	}

	if (matchIdentifier("namespace")) {
		parseNamespace();
		return;
	}

	if (atKeyword("inline") && lookahead().kind == TokenKind::Identifier && lookahead().text == "namespace") {
		throw CompileError("an inline namespace is not supported");
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

	if (tryParseHelperFunction()) {
		return;
	}

	// A file-scope constant. An address space or a type qualifier ahead of the
	// type is what marks one, since a bare "float kX" at file scope is not valid
	// MSL and would otherwise be read as the start of a function's return type.
	if (at(TokenKind::Identifier)
		&& (isTypeQualifier(current().text) || addressSpaceFor(current().text)
			|| isResourceTypeName(current().text)
			|| (current().text == "metal" && lookahead().kind == TokenKind::ColonColon
				&& lookahead(2).kind == TokenKind::Identifier && isResourceTypeName(lookahead(2).text)))) {
		_unit.globals.push_back(parseGlobalDeclaration());
		return;
	}

	throw CompileError("unexpected \"" + std::string(current().text) + "\" at top level; "
		"expected a struct, kernel, vertex, fragment or helper function declaration");
}

// "namespace" is consumed. A name declared in the body is stored under its
// qualified name; an anonymous namespace is the namespace "(anonymous namespace)",
// which its enclosing namespace uses, so its names are visible there unqualified.
void Parser::parseNamespace() {
	const NestingScope nesting(*this);

	const size_t outer = _namespacePath.size();
	if (at(TokenKind::LBrace)) {
		_namespacePath.push_back(kAnonymousNamespace);
		NamespaceScope& parent = _scopes[namespacePrefix(outer)];
		const std::string path = namespacePrefix(outer + 1);
		if (std::find(parent.directives.begin(), parent.directives.end(), path) == parent.directives.end()) {
			parent.directives.push_back(path);
		}
		_namespaces.insert(path);
	} else {
		while (true) {
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a namespace name, found " + std::string(tokenKindName(kind())));
			}
			if (_declared.count(qualify(std::string(current().text)))) {
				throw CompileError("\"" + qualify(std::string(current().text)) + "\" is declared and "
					"cannot also be a namespace");
			}
			_namespacePath.emplace_back(advance().text);
			_namespaces.insert(namespacePrefix(_namespacePath.size()));
			if (!match(TokenKind::ColonColon)) {
				break;
			}
		}
		if (at(TokenKind::Assign)) {
			throw CompileError("a namespace alias is not supported");
		}
		if (at(TokenKind::LBracket)) {
			throw CompileError("an attribute on a namespace is not supported");
		}
	}

	expect(TokenKind::LBrace, "to open a namespace body");
	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		parseDeclaration();
	}
	expect(TokenKind::RBrace, "to close a namespace body");
	_namespacePath.resize(outer);
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

		// Nothing is looked up through the standard library's own namespace, and a
		// file that adds names to it is found by the same spelling.
		if (name.parts.front() == kStdlibNamespace && name.parts.size() == 1) {
			NamespaceScope& entry = _scopes[scope];
			entry.directives.push_back(kStdlibNamespace);
			return;
		}

		const std::string target = namespaceOf(name, name.parts.size());
		if (target.empty()) {
			throw CompileError("\"" + spelled + "\" is not a namespace the file declares");
		}
		_scopes[scope].directives.push_back(target);
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
	if (!_declared.count(resolved) && resolved != "metal::discard_fragment") {
		return; // a name of the standard library, which is visible already
	}
	_scopes[scope].declarations[name.parts.back()] = resolved;
}

static void rejectUnloweredReturn(const Type& returned, const std::string& quoted) {
	if (returned.isPointer || returned.arrayLength || returned.resource != ResourceKind::None
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

bool Parser::tryParseHelperFunction() {
	if (kind() != TokenKind::Identifier && !at(TokenKind::ColonColon)) {
		return false;
	}

	if (atKeyword("template")) {
		throw CompileError("a template is not lowered yet");
	}

	const size_t start = _position;
	FunctionDecl decl;
	decl.line = line();

	try {
		// inline, static and constexpr in any order, ahead of the return type.
		while (atKeyword("inline") || atKeyword("static") || atKeyword("constexpr")) {
			advance();
		}

		decl.returnType = parseType();
		if (kind() != TokenKind::Identifier) {
			_position = start;
			return false;
		}

		decl.name = std::string(advance().text);
		if (!at(TokenKind::LParen)) {
			_position = start;
			return false;
		}
	} catch (const CompileError&) {
		_position = start;
		return false;
	}

	advance();
	decl.order = _functionOrder++;
	decl.globalsBefore = _unit.globals.size();

	const std::string quoted = "helper function \"" + decl.name + "\"";
	rejectUnloweredReturn(decl.returnType, quoted);

	Type builtin;
	if (isTypeName(decl.name, builtin) || isSimdVectorName(decl.name, builtin)) {
		throw CompileError(quoted + " has the name of a builtin type");
	}
	decl.name = qualify(decl.name);
	declareName(decl.name, "function");
	const LocalScope parameters(*this);
	parseHelperParameters(decl, quoted);

	if (at(TokenKind::LBracket)) {
		throw CompileError("attributes on " + quoted + " are not lowered yet");
	}

	if (match(TokenKind::Semicolon)) {
		_unit.helpers.push_back(std::move(decl));
		return true;
	}

	decl.body = parseCompoundStatement();
	_unit.helpers.push_back(std::move(decl));
	return true;
}

VariableDeclaration Parser::parseGlobalDeclaration() {
	VariableDeclaration declaration;

	declaration.type = parseType(true);

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
		return declaration;
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

		StructField field;
		field.type = parseType();
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

// A member that is not a field: a constructor, a member function or a member
// that cannot be lowered. False, with nothing consumed, when the member is a
// field. A function is skipped here and parsed once the struct is complete, since
// its body may name fields declared after it and the struct's own type.
bool Parser::parseStructFunction(StructDecl& decl) {
	const std::string quoted = "\"" + decl.name + "\"";
	if (atKeyword("template")) {
		throw CompileError("a member template is not lowered (in " + quoted + ")");
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
			if (kind() == TokenKind::Identifier) {
				if (current().text == "operator") {
					throw CompileError("an operator overload is not lowered (in " + quoted + ")");
				}
				functionName = std::string(advance().text);
				isFunction = at(TokenKind::LParen);
			}
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
	if (starts.empty()) {
		return;
	}

	_memberStruct = decl.name;
	std::set<std::string> fields;
	for (const StructField& field: decl.fields) {
		fields.insert(field.name);
	}

	const size_t resume = _position;
	_memberFields = &fields;
	for (const size_t start: starts) {
		parseStructMember(decl, start);
	}
	_memberFields = nullptr;
	_memberFunctions.clear();
	_position = resume;
}

namespace {

	ExpressionPtr makeIdentifier(const std::string& name, size_t line) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Identifier;
		expression->line = line;
		expression->name = name;
		return expression;
	}

	// "this.field": the object a member function or constructor works on is a
	// parameter or a local named this.
	ExpressionPtr makeThisMember(const std::string& field, size_t line) {
		auto member = std::make_unique<Expression>();
		member->kind = ExpressionKind::Member;
		member->line = line;
		member->left = makeIdentifier("this", line);
		member->memberName = field;
		member->height = 2;
		return member;
	}

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
		memberName = std::string(advance().text);
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
			ExpressionPtr value = parseExpression();
			expect(TokenKind::RParen, "to close a member initialiser");
			initialisers.emplace_back(field, std::move(value));
		} while (match(TokenKind::Comma));
	}

	if (!isConstructor && !isStatic) {
		if (!matchIdentifier("const")) {
			throw CompileError(quoted + " is not const; mslc takes the object by value, so a member function "
				"that could write it is not lowered (mark it const)");
		}
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
		_unit.helpers.push_back(std::move(fn));
		return;
	}

	fn.body = parseCompoundStatement();
	if (!isConstructor) {
		_unit.helpers.push_back(std::move(fn));
		return;
	}

	if (fn.parameters.empty() && initialisers.empty() && fn.body->children.empty()) {
		constructors->hasDefault = true;
		return;
	}
	if (!constructors->helper.empty()) {
		throw CompileError("overloading the constructors of \"" + decl.name + "\" is not lowered yet");
	}
	constructors->helper = fn.name;
	constructors->parameters = fn.parameters.size();
	constructors->hasDefault = constructors->hasDefault || fn.parameters.empty();

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
	const bool constructs = arguments.empty() ? info.parameters == 0 && !info.helper.empty() : !info.helper.empty();
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

	// An unscoped enum without a fixed underlying type is stored as an int, or as
	// an unsigned int when a value passes INT_MAX, and promotes to that type in
	// arithmetic, so it is that type here. Apple also refuses an int where an enum
	// is expected; mslc does not track that distinction.
	if (const auto enumType = _enumTypes.find(std::string(text)); enumType != _enumTypes.end()) {
		out.scalar = _enumUnderlying.at(enumType->second);
		out.vectorWidth = 0;
		out.matrixColumns = 0;
		out.isPacked = false;
		out.namedType.clear();
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
	out.isConst = out.isConst || alias->second.isConst;
	return true;
}

void Parser::declareName(const std::string& name, const char* what) {
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

void Parser::declareLocal(const std::string& name) {
	// A name that resolves to a declaration inside a namespace is hidden the same way.
	QualifiedName unqualified;
	unqualified.parts = {name};
	const std::string key = resolveName(unqualified);
	const char* what = _enumConstants.count(key) ? "enum constant"
		: _typedefs.count(key) ? "typedef"
		: _enumTypes.count(key) ? "enum type"
		: nullptr;

	if (what) {
		throw CompileError("a variable named \"" + name + "\" would hide the " + std::string(what) + " of "
			"that name, which mslc does not support");
	}

	if (!_locals.empty() && !_locals.back().insert(name).second) {
		throw CompileError("redefinition of \"" + name + "\" in the same scope");
	}
}

bool Parser::isLocal(const std::string& name) const {
	for (const std::set<std::string>& scope : _locals) {
		if (scope.count(name)) {
			return true;
		}
	}
	return false;
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
	const std::string key = scope.empty() ? name : scope + "::" + name;
	if (_declared.count(key)) {
		found.insert(key);
	}
	const auto entry = _scopes.find(scope);
	if (entry == _scopes.end()) {
		return;
	}
	if (const auto brought = entry->second.declarations.find(name); brought != entry->second.declarations.end()) {
		found.insert(brought->second);
	}
}

void Parser::collectMembers(const std::string& scope, const std::string& name, std::set<std::string>& seen,
	std::set<std::string>& found) const {
	if (!seen.insert(scope).second) {
		return;
	}
	collectOwn(scope, name, found);
	const auto entry = _scopes.find(scope);
	if (entry == _scopes.end()) {
		return;
	}
	for (const std::string& directive : entry->second.directives) {
		collectMembers(directive, name, seen, found);
	}
}

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

// The namespace the first "count" parts of a name spell, found from the current
// namespace outward (or from the file, after a leading "::"), or "" when there is none.
std::string Parser::namespaceOf(const QualifiedName& name, size_t count) const {
	const std::vector<std::string>& parts = name.parts;
	const size_t outermost = name.global ? 0 : _namespacePath.size();
	for (size_t depth = outermost + 1; depth-- > 0;) {
		// A namespace that a using directive nominated is searched as well.
		std::vector<std::string> bases = {namespacePrefix(depth)};
		std::set<std::string> seen;
		for (size_t i = 0; i < bases.size(); ++i) {
			const std::string candidate = bases[i].empty() ? parts[0] : bases[i] + "::" + parts[0];
			if (_namespaces.count(candidate)) {
				std::string scope = candidate;
				for (size_t part = 1; part < count; ++part) {
					scope += "::" + parts[part];
					if (!_namespaces.count(scope)) {
						return "";
					}
				}
				return scope;
			}
			if (!seen.insert(bases[i]).second) {
				continue;
			}
			if (const auto entry = _scopes.find(bases[i]); entry != _scopes.end()) {
				bases.insert(bases.end(), entry->second.directives.begin(), entry->second.directives.end());
			}
		}
	}
	return "";
}

std::string Parser::resolveName(const QualifiedName& name) const {
	const auto pick = [&](const std::set<std::string>& found, const std::string& spelled) {
		if (found.size() > 1) {
			throw CompileError("reference to \"" + spelled + "\" is ambiguous: it names " + *found.begin()
				+ " and " + *std::next(found.begin()));
		}
		return *found.begin();
	};

	const std::string& last = name.parts.back();
	if (name.parts.size() == 1 && !name.global) {
		// [namespace.udir]: a using directive in U nominating T makes T's names
		// appear in the nearest namespace L containing both, for lookups from inside U.
		// So each enclosing namespace L, innermost out, offers its own names plus
		// those of directives that land in it; the first level that has any decides.
		for (size_t depth = _namespacePath.size() + 1; depth-- > 0;) {
			const std::string level = joinParts(_namespacePath, 0, depth);
			std::set<std::string> found;
			collectOwn(level, last, found);
			for (size_t user = 0; user <= _namespacePath.size(); ++user) {
				const std::string from = joinParts(_namespacePath, 0, user);
				const auto entry = _scopes.find(from);
				if (entry == _scopes.end()) {
					continue;
				}
				for (const std::string& target : entry->second.directives) {
					if (nearestCommon(from, target) == level) {
						std::set<std::string> seen;
						collectMembers(target, last, seen, found);
					}
				}
			}
			if (!found.empty()) {
				return pick(found, last);
			}
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
		collectMembers(scope, last, seen, members);
		if (!members.empty()) {
			return pick(members, spelled);
		}
	}

	// "metal::sin" is the standard library's "sin", with or without a using
	// directive, unless the file extends the namespace with a name of that spelling.
	if (parts.front() == kStdlibNamespace && parts.size() > 1) {
		if (parts.size() != 2 || last != "discard_fragment") {
			return joinParts(parts, 1, parts.size());
		}
	}

	if (parts.size() == 1) {
		// "::x" names a file-scope declaration, or a builtin.
		return last;
	}
	// "S::member": a member function of a struct is named by the struct.
	if (!found && parts.size() > 1 && !name.global) {
		QualifiedName owner = name;
		owner.parts.pop_back();
		const std::string structName = resolveName(owner);
		if (_unit.findStruct(structName)) {
			return structName + "::" + last;
		}
	}
	if (!found && parts.size() == 2 && parts.front() == kStdlibNamespace && last == "discard_fragment") {
		return "metal::discard_fragment";
	}
	if (!found) {
		throw CompileError("\"" + joinParts(parts, 0, parts.size() - 1) + "\" in \"" + spelled
			+ "\" is not a namespace the file declares");
	}
	throw CompileError("no member named \"" + last + "\" in the namespace \"" + scope + "\" (in \"" + spelled + "\")");
}

std::string Parser::peekResolved(size_t& tokens) const {
	QualifiedName name;
	if (!peekQualifiedName(name)) {
		throw CompileError("expected a name, found " + std::string(tokenKindName(kind())) + " \""
			+ std::string(current().text) + "\"");
	}
	tokens = name.tokens;
	return resolveName(name);
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
	if (!tag.empty()) {
		QualifiedName spelled;
		spelled.parts = {tag};
		tag = hadBody ? qualify(tag) : resolveName(spelled);
	}
	if (!hadBody) {
		return tag;
	}

	advance();
	if (!tag.empty()) {
		declareName(tag, "enum");
		_enumTypes[tag] = tag;
		_enumUnderlying[tag] = ScalarKind::Int;
	}

	// Each enumerator is the previous one plus one unless it says otherwise, and
	// the first is zero. They are kept as 64-bit values and held to what an int or
	// an unsigned int holds, so a value that wrapped could never be mistaken for a
	// small one. An unfixed enum is an int unless a value passes INT_MAX, and then
	// every enumerator of it is an unsigned int, the small ones too (Apple:
	// "kSmall - 4 > 0" is true beside a 0xffffffff).
	int64_t next = 0;
	int64_t lowest = 0;
	int64_t highest = 0;
	std::vector<std::string> names;
	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected an enumerator name, found " + std::string(tokenKindName(kind())));
		}

		const std::string name = qualify(std::string(advance().text));
		int64_t value = next;
		if (match(TokenKind::Assign)) {
			value = evaluateConstant(*parseAssignment(), false);
		}
		if (value > UINT_MAX || value < -INT_MAX) {
			throw CompileError("the enumerator \"" + name + "\" does not fit in an int or an unsigned int");
		}

		declareName(name, "enum constant");
		_enumConstants[name] = {value, ScalarKind::Int};
		names.push_back(name);
		lowest = std::min(lowest, value);
		highest = std::max(highest, value);
		next = value + 1;

		if (!match(TokenKind::Comma)) {
			break;
		}
	}

	expect(TokenKind::RBrace, "to close an enum body");

	ScalarKind underlying = ScalarKind::Int;
	if (highest > INT_MAX) {
		if (lowest < 0) {
			throw CompileError("an enum with a negative enumerator and one past INT_MAX needs a 64-bit type, "
				"which mslc has no enum for");
		}
		underlying = ScalarKind::UInt;
		for (const std::string& name : names) {
			_enumConstants[name].kind = underlying;
		}
	}
	_lastEnumUnderlying = underlying;
	if (!tag.empty()) {
		_enumUnderlying[tag] = underlying;
	}
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
			if (nested && (expression.intKind == ScalarKind::UInt || expression.intKind == ScalarKind::ULong
				|| expression.intValue > INT_MAX)) {
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

			// The right operand is evaluated only when the left does not decide
			// the result, so `0 && 1 / 0` is a constant and not a division by zero.
			if (expression.binaryOperator == BinaryOperator::LogicalAnd && left == 0) {
				return 0;
			}
			if (expression.binaryOperator == BinaryOperator::LogicalOr && left != 0) {
				return 1;
			}

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

// Whether a constant expression has a long or ulong in it, which makes its
// type one. A shift has the type of its left operand alone.
static bool isSixtyFourBit(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
			return expression.intKind == ScalarKind::Long || expression.intKind == ScalarKind::ULong;
		case ExpressionKind::Unary:
			return isSixtyFourBit(*expression.left);
		case ExpressionKind::Binary: {
			const bool isShift = expression.binaryOperator == BinaryOperator::ShiftLeft
				|| expression.binaryOperator == BinaryOperator::ShiftRight;
			return isSixtyFourBit(*expression.left) || (!isShift && isSixtyFourBit(*expression.right));
		}
		default:
			return false;
	}
}

// The index an attribute or an array length takes: a constant that is not
// negative and fits the 32 bits it is stored in. Apple takes an int or a uint
// for an attribute and rejects a long, though an array length may be one.
int64_t Parser::parseConstantIndex(const std::string& context, bool isAttribute) {
	int64_t value = 0;
	try {
		const ExpressionPtr expression = parseAssignment();
		if (isAttribute && isSixtyFourBit(*expression)) {
			throw CompileError("a long is not an index; use an int or a uint");
		}
		value = evaluateConstant(*expression, false);
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

Type Parser::parseType(bool allowResource) {
	Type type;

	// MSL writes the address space and the const qualifier in either order, and
	// both orders are ordinary: "device const float*" and "const device Vertex *"
	// are the same declaration spelled two ways. Reading them as two fixed
	// sequences took "const" as the whole prefix and then read "device" as a type
	// name, so the parameter became "Vertex" and the list ended at the "*".
	while (parseQualifier(type, false)) {
	}

	// metal::sampler and metal::texture2d, which Apple takes as the unqualified names.
	if (kind() == TokenKind::Identifier && current().text == "metal" && lookahead().kind == TokenKind::ColonColon
		&& lookahead(2).kind == TokenKind::Identifier && isResourceTypeName(lookahead(2).text)) {
		advance();
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

	if (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text)) {
		parseSpecifierRun(type);
	} else if (kind() == TokenKind::Identifier && nameTokens == 1 && isResourceTypeName(current().text)) {
		if (!allowResource) {
			throw CompileError("\"" + std::string(current().text) + "\" is a texture or sampler type, "
				"which mslc takes as an entry point parameter only, and a sampler also as a local");
		}
		parseResourceType(type);
	} else if (!name.empty() && resolveTypeName(name, type)) {
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

	while (parseQualifier(type, false)) {
	}

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

	if (name != "texture2d" && name != "texturecube") {
		throw CompileError("\"" + name + "\" is not lowered yet; mslc lowers texture2d<float>, "
			"texture2d<half>, texturecube<float>, texturecube<half> and sampler");
	}

	expect(TokenKind::Less, ("to open the sampled type of " + name).c_str());
	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected the sampled type of " + name + ", found "
			+ std::string(tokenKindName(kind())));
	}

	std::string component(current().text);
	if (isIntegerSpecifier(component)) {
		Type channel;
		parseSpecifierRun(channel);
		component = typeName(channel);
	} else {
		advance();
		Type alias;
		if (!_enumTypes.count(component) && resolveTypeName(component, alias) && !alias.isVector() && !alias.isMatrix() && alias.namedType.empty()) {
			alias.isConst = false;
			component = typeName(alias);
		}
	}
	if (component != "float" && component != "half" && component != "uint") {
		throw CompileError(name + "<" + component + "> is not lowered yet; mslc lowers the "
			"sampled types float and half");
	}

	std::string access = "sample";
	if (match(TokenKind::Comma)) {
		if (kind() == TokenKind::Identifier && current().text == "metal" && lookahead().kind == TokenKind::ColonColon) {
			advance();
			advance();
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
		if (access != "sample" && !(name == "texture2d" && component == "uint" && access == "write")) {
			throw CompileError(name + " access::" + access + " is not lowered yet; mslc lowers "
				"access::sample, which is the default");
		}
	}

	expect(TokenKind::Greater, ("to close the sampled type of " + name).c_str());
	type.resource = name == "texturecube" ? ResourceKind::TextureCube : ResourceKind::Texture2D;
	if (component == "uint" && !(name == "texture2d" && access == "write")) {
		throw CompileError(name + "<uint> is not lowered yet; mslc lowers uint only with texture2d access::write");
	}
	type.textureAccess = access == "write" ? TextureAccess::Write : TextureAccess::Sample;
	type.scalar = component == "uint" ? ScalarKind::UInt
		: component == "half" ? ScalarKind::Half : ScalarKind::Float;
}

Parameter Parser::parseParameter(const std::string& helperName) {
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
	const bool unnamed = helper && (at(TokenKind::Comma) || at(TokenKind::RParen));
	if (!unnamed && kind() != TokenKind::Identifier) {
		throw CompileError("expected a parameter name, found " + std::string(tokenKindName(kind())));
	}

	if (!unnamed) {
		param.name = std::string(advance().text);
		declareLocal(param.name);
	}

	if (helper) {
		const std::string what = "parameter \"" + param.name + "\" of helper function \"" + helperName + "\" ";
		if (param.type.isPointer) {
			throw CompileError(what + "is a pointer, which a helper function does not take yet; "
				"it takes scalar, vector, matrix and struct values");
		}
		if (param.type.arrayLength || (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket)) {
			throw CompileError(what + "is an array, which a helper function does not take yet");
		}
		// A reference to a thread variable may spell its space out; any other
		// parameter has none to name.
		if (param.type.addressSpace != AddressSpace::None
			&& !(param.isReference && param.type.addressSpace == AddressSpace::Thread)) {
			throw CompileError(what + "is in the " + addressSpaceName(param.type.addressSpace)
				+ " address space, which a helper function does not take yet");
		}
		if (at(TokenKind::LBracket)) {
			throw CompileError(what + "has an attribute, which only an entry point's parameter can have");
		}
		return param;
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

	parseAttributeList([&](const std::string& name, std::optional<uint32_t> argument, const std::string&) {
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
		} else if (name == "depth") {
			throw CompileError("[[depth]] is not valid on an input parameter");
		} else if (name == "sample_mask") {
			throw CompileError("[[sample_mask]] on an input parameter is not lowered yet");
		} else if (name == "stage_in") {
			attributes.stageIn = true;
		} else if (name == "color") {
			throw CompileError("[[color(n)]] on a parameter is framebuffer fetch, which mslc does not "
				"lower: Vulkan reads a previous colour attachment only through a subpass input "
				"attachment, which needs a descriptor mslc does not assign");
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

	parseAttributeList([&](const std::string& name, std::optional<uint32_t> argument,
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
		} else if (const auto interpolation = interpolationFromName(name)) {
			if (attributes.interpolation != Interpolation::None) {
				throw CompileError("a struct field has more than one interpolation attribute; "
					"\"" + name + "\" follows another");
			}
			attributes.interpolation = *interpolation;
		} else if (builtinFromName(name)) {
			throw CompileError("builtin attribute \"" + name + "\" is not valid on a struct "
				"field; only [[position]], [[attribute(n)]], [[color(n)]], [[depth(mode)]], [[sample_mask]], [[user(name)]] and the "
				"interpolation attributes are");
		} else {
			throw CompileError("unsupported attribute \"" + name + "\" on a struct field");
		}
	});

	return attributes;
}

void Parser::parseAttributeList(const std::function<void(const std::string&, std::optional<uint32_t>,
	const std::string&)>& visit) {
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
		std::string identifier;
		if (at(TokenKind::LParen)) {
			if (name == "sample_mask") {
				throw CompileError("[[sample_mask]] takes no argument");
			}
			advance();
			if (name == "user" || name == "depth") {
				if (kind() != TokenKind::Identifier) {
					throw CompileError("attribute \"" + name + "\" needs an identifier, found "
						+ std::string(tokenKindName(kind())));
				}
				identifier = std::string(advance().text);
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

// The initialiser a local may have after its name: "= value", a braced list,
// or constructor arguments. A const local has to have one.
void Parser::parseLocalInitializer(VariableDeclaration& declaration) {
	if (declaration.type.isConst && !declaration.type.isPointer && !at(TokenKind::Assign)
		&& !at(TokenKind::LParen) && !at(TokenKind::LBracket) && !at(TokenKind::LBrace)) {
		throw CompileError("the const variable \"" + declaration.name + "\" needs an initialiser");
	}

	if (match(TokenKind::Assign)) {
		declaration.initializer = parseInitializer();
	} else if (at(TokenKind::LBrace)) {
		declaration.initializer = parseInitializerList();
	} else if (at(TokenKind::LParen)) {
		// Direct initialisation, float3 specularTerm(0); . The name comes
		// first here, so the list after it constructs the variable's own
		// type rather than calling it, and the two spellings are the same
		// value. Metal has no other form: "float3(0) specularTerm;" is a
		// parse error there, which xcrun metal confirms.
		advance();
		const size_t atLine = line();
		std::vector<ExpressionPtr> arguments = parseArgumentList("to close a constructor's argument list");
		declaration.initializer = constructWithConstructor(declaration.type, arguments, atLine);
		if (!declaration.initializer) {
			auto construct = std::make_unique<Expression>();
			construct->kind = ExpressionKind::Construct;
			construct->line = atLine;
			construct->constructType = declaration.type;
			construct->arguments = std::move(arguments);
			measure(*construct);
			declaration.initializer = std::move(construct);
		}
	} else {
		std::vector<ExpressionPtr> none;
		declaration.initializer = constructWithConstructor(declaration.type, none, line());
	}
}

void Parser::appendStatement(std::vector<StatementPtr>& out) {
	out.push_back(parseStatement());
	for (StatementPtr& tail: _declaratorTail) {
		out.push_back(std::move(tail));
	}
	_declaratorTail.clear();
}

StatementPtr Parser::parseBody() {
	if (at(TokenKind::LBrace)) {
		return parseCompoundStatement();
	}

	const NestingScope scope(*this);
	const LocalScope locals(*this);
	StatementPtr body = parseStatement();
	if (_declaratorTail.empty() && body->kind != StatementKind::DeclarationStatement) {
		return body;
	}

	// "if (c) int a, b;" is one declaration in a scope of its own, and so is a
	// lone one: without a block its name would replace the enclosing one.
	auto block = std::make_unique<Statement>();
	block->kind = StatementKind::Compound;
	block->line = body->line;
	block->children.push_back(std::move(body));
	for (StatementPtr& tail: _declaratorTail) {
		block->children.push_back(std::move(tail));
	}
	_declaratorTail.clear();
	return block;
}

StatementPtr Parser::parseCompoundStatement() {
	const NestingScope scope(*this);
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Compound;
	statement->line = line();
	const LocalScope locals(*this);

	expect(TokenKind::LBrace, "to open a block");

	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		appendStatement(statement->children);
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

	// "enum Mode m;" declares a local of an enum declared earlier; anything else
	// that starts with "enum" would declare a type.
	const bool elaboratedEnumLocal = atKeyword("enum") && lookahead().kind == TokenKind::Identifier
		&& _enumTypes.count(std::string(lookahead().text)) && lookahead(2).kind == TokenKind::Identifier;
	if (atKeyword("typedef") || (atKeyword("enum") && !elaboratedEnumLocal)) {
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

	if (atKeyword("switch")) {
		return parseSwitchStatement();
	}

	if (atKeyword("return")) {
		return parseReturnStatement();
	}

	QualifiedName discardName;
	if (isDiscardFragment(discardName)) {
		statement->kind = StatementKind::Discard;
		for (size_t i = 0; i < discardName.tokens; ++i) {
			advance();
		}
		expect(TokenKind::LParen, "to open the call \"discard_fragment()\"");
		expect(TokenKind::RParen, "to close the call \"discard_fragment()\", which takes no arguments");
		expect(TokenKind::Semicolon, "after \"discard_fragment()\"");
		return statement;
	}

	if (atKeyword("break") || atKeyword("continue")) {
		const std::string keyword(advance().text);
		statement->kind = keyword == "break" ? StatementKind::Break : StatementKind::Continue;
		expect(TokenKind::Semicolon, "after a jump statement");
		return statement;
	}

	// The one attribute a statement carries is the fall-through marker, which
	// lowers to nothing: a case's block already branches to the next, and the
	// attribute only tells the reader the fall-through is meant.
	if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket
		&& lookahead(2).kind == TokenKind::Identifier && lookahead(2).text == "fallthrough"
		&& lookahead(3).kind == TokenKind::RBracket && lookahead(4).kind == TokenKind::RBracket) {
		advance();
		advance();
		advance();
		advance();
		advance();
		expect(TokenKind::Semicolon, "after a [[fallthrough]] attribute");
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
	}

	// A declaration statement starts with a type. Distinguish it from an
	// expression by looking for a bare type name, a type qualifier or a known
	// address space before anything that could begin an expression. A struct
	// declared earlier in the unit counts, since "Vertex vtx;" is a declaration
	// and nothing else could start with those two words.
	{
		Type probe;
		size_t nameTokens = 0;
		const std::string name = kind() == TokenKind::Identifier || at(TokenKind::ColonColon)
			? peekResolved(nameTokens) : std::string();
		const bool looksLikeType =
			((kind() == TokenKind::Identifier || at(TokenKind::ColonColon)) && (resolveTypeName(name, probe)
				|| isTypeQualifier(name) || isResourceTypeName(name)
				|| _unit.findStruct(name) != nullptr))
			|| (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text))
			|| atKeyword("enum") || atKeyword("device") || atKeyword("constant")
			|| atKeyword("threadgroup") || atKeyword("thread");

		if (looksLikeType) {
			statement->kind = StatementKind::DeclarationStatement;
			VariableDeclaration declaration;
			declaration.type = parseType(true);
			declaration.isReference = at(TokenKind::Ampersand);
			rejectLocalQualifiers(declaration.type, declaration.isReference, true);
			if (declaration.isReference) {
				advance();
				if (declaration.type.isPointer || declaration.type.arrayLength
					|| declaration.type.resource != ResourceKind::None) {
					throw CompileError("a reference to a pointer, an array or a resource is not lowered yet");
				}
			}

			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
			}
			declaration.name = std::string(advance().text);
			declareLocal(declaration.name);

			if (declaration.isReference) {
				if (!match(TokenKind::Assign)) {
					throw CompileError("the reference \"" + declaration.name + "\" needs an initialiser");
				}
				declaration.initializer = parseExpression();
				statement->declaration = std::move(declaration);
				expect(TokenKind::Semicolon, "after a reference declaration");
				return statement;
			}

			if (declaration.type.resource != ResourceKind::None) {
				parseSamplerLocal(declaration);
				statement->declaration = std::move(declaration);
				return statement;
			}

			parseLocalInitializer(declaration);

			// "T a, b = 1, c;" declares each name with the type the first one has.
			// A star binds to the name it precedes, so a pointer would need its own
			// rule, and locals of pointer type are not lowered anyway.
			while (at(TokenKind::Comma)) {
				if (declaration.type.isPointer || declaration.type.arrayLength) {
					throw CompileError("several declarators after a pointer or an array are not lowered yet");
				}
				advance();
				auto next = std::make_unique<Statement>();
				next->kind = StatementKind::DeclarationStatement;
				next->line = line();
				VariableDeclaration another;
				another.type = declaration.type;
				if (kind() != TokenKind::Identifier) {
					throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
				}
				another.name = std::string(advance().text);
				declareLocal(another.name);
				parseLocalInitializer(another);
				next->declaration = std::move(another);
				_declaratorTail.push_back(std::move(next));
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
		|| (type.addressSpace != AddressSpace::None && type.addressSpace != AddressSpace::Thread
			&& type.addressSpace != AddressSpace::Constant)) {
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

	statement->thenBranch = parseBody();

	if (matchIdentifier("else")) {
		statement->elseBranch = parseBody();
	}

	return statement;
}

StatementPtr Parser::parseForStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::For;
	statement->line = line();
	const LocalScope locals(*this);

	expectKeyword("for", "at the start of a for");
	expect(TokenKind::LParen, "after 'for'");

	{
		Type probe;
		size_t nameTokens = 0;
		const std::string name = kind() == TokenKind::Identifier || at(TokenKind::ColonColon)
			? peekResolved(nameTokens) : std::string();
		const bool looksLikeType = (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text))
			|| ((kind() == TokenKind::Identifier || at(TokenKind::ColonColon))
				&& (resolveTypeName(name, probe) || _unit.findStruct(name) != nullptr));

		if (looksLikeType) {
			VariableDeclaration declaration;
			declaration.type = parseType();
			rejectLocalQualifiers(declaration.type, at(TokenKind::Ampersand));
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a loop variable name in a for initialiser");
			}
			declaration.name = std::string(advance().text);
			declareLocal(declaration.name);
			if (match(TokenKind::Assign)) {
				declaration.initializer = parseInitializer();
			} else if (at(TokenKind::LBrace)) {
				declaration.initializer = parseInitializerList();
			}
			statement->forInitializer = std::move(declaration);
		} else if (!at(TokenKind::Semicolon)) {
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

	statement->forBody = parseBody();

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

	statement->whileBody = parseBody();

	return statement;
}

// A switch parses into a flat list of labels, each holding the statements up
// to the next label: the block structure the lowering wants is one block per
// label, branching to the next for a fall-through. Statements before the first
// label are reachable from nowhere, as C++ says, and are kept in the preamble.
StatementPtr Parser::parseSwitchStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Switch;
	statement->line = line();
	const LocalScope locals(*this);

	expectKeyword("switch", "at the start of a switch");
	expect(TokenKind::LParen, "after 'switch'");
	statement->expression = parseExpression();
	expect(TokenKind::RParen, "after a switch selector");

	// C++ takes any statement for a body, but a case label can only live inside
	// a block, so the block is the only body that holds a switch's meaning.
	if (!at(TokenKind::LBrace)) {
		throw CompileError("a switch body needs braces around its case labels");
	}
	advance();

	SwitchCase* kase = nullptr;
	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		if (atKeyword("case")) {
			advance();
			SwitchCase next;
			next.line = line();
			next.value = parseAssignment();
			expect(TokenKind::Colon, "after a case value");
			statement->switchCases.push_back(std::move(next));
			kase = &statement->switchCases.back();
			continue;
		}
		if (atKeyword("default")) {
			advance();
			SwitchCase next;
			next.line = line();
			expect(TokenKind::Colon, "after 'default'");
			statement->switchCases.push_back(std::move(next));
			kase = &statement->switchCases.back();
			continue;
		}

		appendStatement(kase ? kase->body : statement->switchPreamble);
	}

	expect(TokenKind::RBrace, "to close a switch body");

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
	const NestingScope scope(*this);
	auto left = parseConditional();

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
		measure(*expression);
		return expression;
	}

	return left;
}











// `c ? a : b` binds looser than ||, and right to left: the false arm is itself
// an assignment-expression, so `c ? a : d ? b : e` nests to the right and
// `c ? a : x = b` assigns inside the arm, as C++ parses it.
ExpressionPtr Parser::parseConditional() {
	auto condition = parseLogicalOr();
	if (!at(TokenKind::Question)) {
		return condition;
	}

	advance();
	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::Conditional;
	expression->line = condition->line;
	expression->left = std::move(condition);
	expression->arguments.push_back(parseExpression());
	expect(TokenKind::Colon, "between the two values of a conditional");
	expression->arguments.push_back(parseAssignment());
	measure(*expression);
	return expression;
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
	// "*this" is the object itself, which a member function holds by value.
	if (at(TokenKind::Star) && lookahead().kind == TokenKind::Identifier && lookahead().text == "this"
		&& isLocal("this")) {
		advance();
		return parseUnary();
	}

	// Only in prefix position: a '*' between two operands never gets here, because
	// parseMultiplicative consumes it after the left operand is complete.
	if (at(TokenKind::Star)) {
		auto dereference = std::make_unique<Expression>();
		dereference->kind = ExpressionKind::Index;
		dereference->isDereference = true;
		dereference->line = line();
		advance();

		const NestingScope scope(*this);
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
		measure(*dereference);
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
		const NestingScope scope(*this);
		expression->left = parseUnary();
		measure(*expression);
		return expression;
	}

	if (at(TokenKind::LParen)) {
		if (auto cast = parseCast()) {
			return cast;
		}
	}

	return parsePostfix();
}

// A nonconsuming null result lets a speculative C-style cast backtrack.
std::optional<Type> Parser::peekCastTargetName(size_t& nameTokens) {
	const std::string head(current().text);
	if (kind() == TokenKind::Identifier) {
		if (isTypeQualifier(head) || addressSpaceFor(head)) {
			throw CompileError("a cast to a qualified, pointer or reference type is not supported");
		}
		if (head == "void") {
			throw CompileError("a cast to void is not supported");
		}
	}

	QualifiedName spelled;
	if (!peekQualifiedName(spelled)) {
		return std::nullopt;
	}
	const std::string name = peekResolved(nameTokens);

	Type type;
	if (!resolveTypeName(name, type)) {
		if (!_unit.findStruct(name)) {
			return std::nullopt;
		}
		type.namedType = name;
	}

	return type;
}

// `(T)operand`, where T names a scalar, vector or matrix type, a typedef, an
// enum or a struct, is the functional cast T(operand). A type name cannot begin a
// parenthesised expression, so "(" followed by one is always a cast. Returns null
// when the parenthesis opens an expression.
ExpressionPtr Parser::parseCast() {
	const Token& first = lookahead(1);
	if (first.kind != TokenKind::Identifier && first.kind != TokenKind::ColonColon) {
		return nullptr;
	}

	// "(unsigned int)x" and "(const unsigned char)x": a run of integer specifier
	// words, which only a type can begin. A qualifier may come first.
	{
		size_t skip = 1;
		while (lookahead(skip).kind == TokenKind::Identifier
			&& (lookahead(skip).text == "const" || lookahead(skip).text == "volatile")) {
			++skip;
		}
		if (lookahead(skip).kind == TokenKind::Identifier && isIntegerSpecifier(lookahead(skip).text)) {
			advance();
			Type type;
			parseSpecifierRun(type);
			if (at(TokenKind::Star) || at(TokenKind::Ampersand) || at(TokenKind::Less)) {
				throw CompileError("a cast to a pointer, reference or template type is not supported");
			}
			expect(TokenKind::RParen, "to close a cast");
			auto expression = std::make_unique<Expression>();
			expression->kind = ExpressionKind::Construct;
			expression->line = line();
			expression->constructType = type;
			const NestingScope scope(*this);
			expression->arguments.push_back(parseUnary());
			measure(*expression);
			return expression;
		}
	}

	// Read the name, which may be namespace-qualified, from just past the '('.
	const size_t open = _position;
	++_position;
	size_t nameTokens = 0;
	std::optional<Type> type;
	try {
		type = peekCastTargetName(nameTokens);
	} catch (...) {
		_position = open;
		throw;
	}
	_position = open;
	if (!type) {
		return nullptr;
	}

	const Token& after = lookahead(1 + nameTokens);
	if (after.kind == TokenKind::Star || after.kind == TokenKind::Ampersand || after.kind == TokenKind::Less) {
		throw CompileError("a cast to a pointer, reference or template type is not supported");
	}
	if (after.kind != TokenKind::RParen) {
		return nullptr;
	}

	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::Construct;
	expression->line = line();
	expression->constructType = *type;
	advance();
	for (size_t i = 0; i < nameTokens; ++i) {
		advance();
	}
	advance();
	const NestingScope scope(*this);
	expression->arguments.push_back(parseUnary());
	measure(*expression);
	return expression;
}

// Unlike a C-style cast, a named cast cannot backtrack into an expression.
ExpressionPtr Parser::parseNamedCast() {
	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::Construct;
	expression->line = line();

	advance();
	advance();

	Type type;
	while (kind() == TokenKind::Identifier
		&& (current().text == "const" || current().text == "volatile")) {
		advance();
	}
	if (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text)) {
		parseSpecifierRun(type);
	} else {
		size_t nameTokens = 0;
		auto target = peekCastTargetName(nameTokens);
		if (!target) {
			if (nameTokens > 0 && lookahead(nameTokens).kind == TokenKind::Less) {
				throw CompileError("a cast to a pointer, reference or template type is not supported");
			}
			throw CompileError("expected a type name in \"static_cast<\", found \""
				+ std::string(current().text) + "\"");
		}
		type = *target;
		for (size_t i = 0; i < nameTokens; ++i) {
			advance();
		}
	}

	if (at(TokenKind::Star) || at(TokenKind::Ampersand) || at(TokenKind::Less)) {
		throw CompileError("a cast to a pointer, reference or template type is not supported");
	}
	expect(TokenKind::Greater, "to close the type of a static_cast");
	expect(TokenKind::LParen, "for the operand of a static_cast");
	{
		const NestingScope scope(*this);
		expression->arguments.push_back(parseExpression());
	}
	expect(TokenKind::RParen, "to close a static_cast");
	if (auto call = constructWithConstructor(type, expression->arguments, expression->line)) {
		return call;
	}
	expression->constructType = type;
	measure(*expression);
	return expression;
}

ExpressionPtr Parser::parsePostfix() {
	auto expression = parsePrimary();

	while (true) {
		if (at(TokenKind::Increment) || at(TokenKind::Decrement)) {
			auto step = std::make_unique<Expression>();
			step->kind = ExpressionKind::Unary;
			step->line = expression->line;
			step->unaryOperator = at(TokenKind::Increment)
				? UnaryOperator::PostIncrement : UnaryOperator::PostDecrement;
			advance();
			step->left = std::move(expression);
			measure(*step);
			return step;
		}

		if (at(TokenKind::LBracket)) {
			advance();
			auto index = std::make_unique<Expression>();
			index->kind = ExpressionKind::Index;
			index->line = expression->line;
			index->left = std::move(expression);
			index->arguments.push_back(parseExpression());
			expect(TokenKind::RBracket, "to close an index");
			measure(*index);
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
			measure(*member);
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
			measure(*call);
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

// The type of an integer literal, as C gives it and as Apple's compiler does.
// An unsuffixed decimal is the first of int and long that holds it, where hex
// and octal go on through uint and ulong; a decimal never becomes unsigned
// without a `u`, and one too large for a long wraps, which is what Apple
// compiles it to.
static ScalarKind integerLiteralKind(const Token& literal) {
	if (literal.integerDigitsInvalid) {
		throw CompileError("invalid digit in integer constant \"" + std::string(literal.text) + "\"");
	}

	if (literal.integerSuffixInvalid) {
		size_t digitsEnd = literal.text.size();
		while (digitsEnd > 0 && (std::string_view("uUlL").find(literal.text[digitsEnd - 1]) != std::string_view::npos)) {
			--digitsEnd;
		}
		throw CompileError("invalid suffix \"" + std::string(literal.text.substr(digitsEnd))
			+ "\" on integer constant");
	}

	if (literal.integerOverflows) {
		throw CompileError("integer literal is too large to be represented in any integer type");
	}

	const uint64_t value = literal.integerValue;
	if (literal.integerIsUnsigned) {
		return literal.integerLongCount == 0 && value <= UINT_MAX ? ScalarKind::UInt : ScalarKind::ULong;
	}

	// An l is a long, and a long long is one here too. Apple wraps a decimal or
	// a long long that is too large rather than making it unsigned, and only a
	// hex or octal l goes on to ulong.
	if (literal.integerLongCount > 0) {
		const bool wraps = literal.integerIsDecimal || literal.integerLongCount == 2;
		return wraps || value <= INT64_MAX ? ScalarKind::Long : ScalarKind::ULong;
	}

	if (value <= INT_MAX) {
		return ScalarKind::Int;
	}

	// A decimal from 2^63 to 2^64 - 1 is a long holding the wrapped bits, as in
	// Apple's compiler, which says nothing about a bare literal and only warns
	// when it is narrowed into a variable. C would make it unsigned.
	if (literal.integerIsDecimal) {
		return ScalarKind::Long;
	}

	if (value <= UINT_MAX) {
		return ScalarKind::UInt;
	}

	return value <= INT64_MAX ? ScalarKind::Long : ScalarKind::ULong;
}

ExpressionPtr Parser::parsePrimary() {
	// mslc lowers no string type, so a string in expression position is a
	// rejection. Naming it as one is the whole point: read as a name, it
	// reported a failed lookup for an identifier spelled with quotes.
	if (at(TokenKind::StringLiteral)) {
		throw CompileError("a string literal is not lowered; mslc has no string type");
	}

	if (kind() == TokenKind::Identifier && lookahead().kind == TokenKind::Less) {
		if (atKeyword("static_cast")) {
			return parseNamedCast();
		}
		if (atKeyword("reinterpret_cast") || atKeyword("const_cast")) {
			throw CompileError(std::string(current().text) + " is not supported; "
				"use static_cast or a functional cast for a value conversion");
		}
	}

	if (at(TokenKind::IntegerLiteral)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::IntLiteral;
		expression->line = line();
		const Token literal = advance();
		expression->intValue = literal.integerValue;
		expression->intKind = integerLiteralKind(literal);
		return expression;
	}

	if (at(TokenKind::FloatLiteral)) {
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::FloatLiteral;
		expression->line = line();
		const Token literal = advance();
		expression->floatValue = literal.floatValue;
		expression->floatIsHalf = literal.floatIsHalf;
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

	if (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text)) {
		// unsigned(x) and short(x) are functional casts; a run of more words is
		// not one, as in C++, and Apple rejects it.
		Type type;
		if (parseSpecifierRun(type) > 1) {
			throw CompileError("a functional cast takes a single-word type name; write (" + typeName(type)
				+ ")x instead");
		}
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Construct;
		expression->line = line();
		expression->constructType = type;
		if (!at(TokenKind::LParen)) {
			throw CompileError("expected '(' after type \"" + typeName(type) + "\"");
		}
		advance();
		expression->arguments = parseArgumentList("to close a constructor's argument list");
		measure(*expression);
		return expression;
	}

	if (kind() == TokenKind::Identifier || at(TokenKind::ColonColon)) {
		// A name written with namespaces is read whole; a plain one is looked up
		// unless a parameter or local of that name is in scope.
		size_t spelledTokens = 1;
		QualifiedName spelled;
		if (peekQualifiedName(spelled) && spelled.global && spelled.parts.size() == 1
			&& isLocal(spelled.parts[0])) {
			throw CompileError("\"::" + spelled.parts[0] + "\" names the file-scope declaration, which the local "
				+ "of that name hides here; mslc keeps the two apart only under a namespace name");
		}
		if (_memberFields && kind() == TokenKind::Identifier && !isLocal(std::string(current().text))
			&& _memberFields->count(std::string(current().text)) && isLocal("this")) {
			const size_t atLine = line();
			auto member = makeThisMember(std::string(advance().text), atLine);
			return member;
		}
		// A member function named bare calls that function on this, or the static
		// one of the struct.
		if (_memberFields && kind() == TokenKind::Identifier && !isLocal(std::string(current().text))
			&& lookahead().kind == TokenKind::LParen) {
			const auto function = _memberFunctions.find(std::string(current().text));
			if (function != _memberFunctions.end()) {
				const size_t atLine = line();
				const std::string name(advance().text);
				if (function->second) {
					return makeIdentifier(_memberStruct + "::" + name, atLine);
				}
				return makeThisMember(name, atLine);
			}
		}
		const std::string resolved = kind() == TokenKind::Identifier && isLocal(std::string(current().text))
			? std::string(current().text) : peekResolved(spelledTokens);
		const auto consumeName = [&] {
			for (size_t i = 0; i < spelledTokens; ++i) {
				advance();
			}
		};
		const std::string_view text = resolved;

		Type type;
		if (resolveTypeName(text, type) && type.namedType.empty()) {
			// A type name in expression position constructs a value: float(x),
			// float3(0), float4(a, b, c, 1). T(...) is a constructor call, and the
			// parenthesised part is a list of arguments; (T)x is parseCast's.
			consumeName();

			if (!at(TokenKind::LParen)) {
				throw CompileError("expected '(' after type \"" + std::string(text) + "\"");
			}

			advance();
			auto expression = std::make_unique<Expression>();
			expression->kind = ExpressionKind::Construct;
			expression->line = line();
			expression->constructType = type;
			expression->arguments = parseArgumentList("to close a constructor's argument list");
			measure(*expression);
			return expression;
		}

		// A struct's name and an argument list call its constructor.
		if (!isLocal(resolved) && _constructors.count(resolved) && lookahead(spelledTokens).kind == TokenKind::LParen) {
			const size_t atLine = line();
			consumeName();
			advance();
			Type type;
			type.namedType = resolved;
			std::vector<ExpressionPtr> arguments = parseArgumentList("to close a constructor's argument list");
			ExpressionPtr call = constructWithConstructor(type, arguments, atLine);
			if (!call) {
				throw CompileError("\"" + resolved + "\" has a constructor with nothing to do, and a value of it is not lowered");
			}
			return call;
		}

		// An enumerator is the integer it was declared with.
		const auto constant = _enumConstants.find(resolved);
		if (constant != _enumConstants.end()) {
			auto expression = std::make_unique<Expression>();
			expression->line = line();
			consumeName();
			expression->kind = ExpressionKind::IntLiteral;
			const int64_t value = constant->second.value;
			expression->intKind = constant->second.kind;
			expression->intValue = static_cast<uint64_t>(value < 0 ? -value : value);
			if (value >= 0) {
				return expression;
			}

			auto negated = std::make_unique<Expression>();
			negated->kind = ExpressionKind::Unary;
			negated->line = expression->line;
			negated->unaryOperator = UnaryOperator::Negate;
			negated->left = std::move(expression);
			measure(*negated);
			return negated;
		}

		// A name that is also an MSL builtin is a plain identifier here. Whether
		// it is the builtin or a parameter that shadows one is a question about
		// scope, which the parser has no answer for, so the emitter decides.
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Identifier;
		expression->line = line();
		expression->name = resolved;
		consumeName();
		return expression;
	}

	throw CompileError("unexpected " + std::string(tokenKindName(kind())) + " \""
		+ std::string(current().text) + "\" in an expression");
}

}
