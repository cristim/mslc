#include <cmath>
#include <limits>
#include "parser.h"
#include "layout.h"

#include <algorithm>
#include <climits>
#include <map>
#include <typeinfo>
#include <unordered_map>

namespace mslc {

namespace {
	// Thrown to stop a re-read of the file where a template's declaration ends.
	struct StopParsing {};

	constexpr const char* kStdlibNamespace = "metal";
	constexpr const char* kAnonymousNamespace = "(anonymous namespace)";

	std::string joinParts(const std::vector<std::string>& parts, size_t from, size_t to) {
		std::string out;
		for (size_t i = from; i < to; ++i) {
			out += (out.empty() ? "" : "::") + parts[i];
		}
		return out;
	}

	// Whether the standard library has a namespace or enum "scope" ("metal", "metal::precise",
	// "metal::access"), and a name in it: stdlib_names.inc holds one line per scope, led by it.
	struct StdlibLookup {
		bool scopeExists = false;
		bool hasName = false;
	};

	StdlibLookup stdlibLookup(std::string_view scope, std::string_view name) {
		static const std::string_view table =
#include "stdlib_names.inc"
			;
		StdlibLookup result;
		const std::string lead = std::string(scope) + " ";
		const std::string needle = " " + std::string(name) + " ";
		for (size_t at = 0; at < table.size();) {
			const size_t end = std::min(table.find('\n', at), table.size());
			const std::string line = " " + std::string(table.substr(at, end - at)) + " ";
			if (line.compare(1, lead.size(), lead) == 0) {
				result.scopeExists = true;
				result.hasName = result.hasName || line.find(needle, lead.size()) != std::string::npos;
			}
			at = end + 1;
		}
		return result;
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
			{ "size_t", ScalarKind::ULong },
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

	bool isMetalBuiltinTypeName(std::string_view text) {
		ScalarKind kind;
		if (isScalarTypeName(text, kind)) {
			return text.front() == 'u' || text.substr(text.size() - 2) == "_t";
		}
		if (text == "atomic_int" || text == "atomic_uint") {
			return true;
		}
		const bool packed = text.substr(0, 7) == "packed_";
		if (packed) {
			text.remove_prefix(7);
		}
		if (text.size() < 2 || text.back() < '2' || text.back() > '4') {
			return false;
		}
		if (text.size() > 3 && text[text.size() - 2] == 'x') {
			const auto leaf = text.substr(0, text.size() - 3);
			return !packed && (leaf == "float" || leaf == "half")
				&& text[text.size() - 3] >= '2' && text[text.size() - 3] <= '4';
		}
		const auto leaf = text.substr(0, text.size() - 1);
		return leaf.find('_') == std::string_view::npos && isScalarTypeName(leaf, kind)
			&& kind != ScalarKind::Void && kind != ScalarKind::Double;
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

	// The texture and sampler type names. Only texture1d, texture2d, texturecube, texture3d,
	// texture2d_array and sampler are lowered; the rest are recognised so they are
	// reported by name rather than read as a struct type and failing later.
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
		if (name == "threads_per_threadgroup") { return Builtin::ThreadsPerThreadgroup; }
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
static bool tagBesideObject(std::string_view a, std::string_view b) {
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
static bool tagBesideNonTag(std::string_view a, std::string_view b) {
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

static void rejectUnloweredReturn(const Type& returned, const std::string& quoted, bool allowStdArray = false) {
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

// The parameters of a template, from the "<" to the ">" that closes them.
void Parser::parseTemplateHeader(FunctionTemplate& declaration) {
	expect(TokenKind::Less, "to open a template parameter list");
	std::set<std::string> names;
	for (;;) {
		TemplateParameter parameter;
		if (atKeyword("template")) {
			throw CompileError("a template template parameter is not lowered");
		}
		if (at(TokenKind::Ellipsis)) {
			throw CompileError("a template parameter pack is not lowered");
		}
		if (atKeyword("typename") || atKeyword("class")) {
			advance();
			if (at(TokenKind::Ellipsis)) {
				throw CompileError("a template parameter pack is not lowered");
			}
		} else {
			// A non-type parameter: an integer or a bool. Any other type (a parameter whose type is
			// another parameter, an enum, a pointer) is a template mslc does not instantiate.
			parameter.isType = false;
			const Type type = parseType();
			if (!type.namedType.empty() || type.isVector() || type.isMatrix() || type.isPointer || type.arrayLength
				|| type.resource != ResourceKind::None || !type.enumName.empty() || type.isAtomic || type.isPacked
				|| type.scalar == ScalarKind::Void || type.scalar == ScalarKind::Half || type.scalar == ScalarKind::Float
				|| type.scalar == ScalarKind::Double) {
				throw CompileError("a non-type template parameter of that type is not lowered; mslc takes an "
					"integer or a bool");
			}
			parameter.kind = type.scalar;
			if (at(TokenKind::Ellipsis)) {
				throw CompileError("a template parameter pack is not lowered");
			}
		}
		if (kind() == TokenKind::Identifier) {
			parameter.name = std::string(advance().text);
			if (!names.insert(parameter.name).second) {
				throw CompileError("redeclaration of the template parameter \"" + parameter.name + "\"");
			}
		}
		if (match(TokenKind::Assign)) {
			// The default's tokens, up to the "," or ">" that ends it. A "<" inside it opens a nested
			// list; the same ">>" is two closers, and one that would close the header too is refused.
			parameter.hasDefault = true;
			const size_t first = _position;
			size_t angles = 0;
			for (;;) {
				if (at(TokenKind::EndOfFile)) {
					throw CompileError("expected '>' to close a template parameter list");
				}
				if (at(TokenKind::Less)) {
					++angles;
				} else if (at(TokenKind::Greater)) {
					if (angles == 0) {
						break;
					}
					--angles;
				} else if (at(TokenKind::ShiftRight)) {
					if (angles == 0) {
						throw CompileError("unbalanced '>>' in a template parameter list");
					}
					if (angles == 1) {
						// One '>' closes the default's own list and the other the header's: split the token.
						_tokens[_position].kind = TokenKind::Greater;
						_tokens[_position].text = ">";
						_tokens.insert(_tokens.begin() + _position, _tokens[_position]);
						continue;
					}
					angles -= 2;
				} else if (at(TokenKind::Comma) && angles == 0) {
					break;
				} else if (at(TokenKind::LParen)) {
					skipBalanced(TokenKind::LParen, TokenKind::RParen);
					continue;
				}
				advance();
			}
			parameter.defaultTokens.assign(_tokens.begin() + first, _tokens.begin() + _position);
			parameter.defaultTokens.emplace_back();
		}
		declaration.parameters.push_back(std::move(parameter));
		if (match(TokenKind::Comma)) {
			continue;
		}
		if (at(TokenKind::Greater)) {
			advance();
			return;
		}
		throw CompileError("expected '>' to close a template parameter list");
	}
}

namespace {

Stage stageForKeyword(std::string_view word) {
	return word == "kernel" ? Stage::Kernel : word == "vertex" ? Stage::Vertex
		: word == "fragment" ? Stage::Fragment : Stage::None;
}

}

// The declarator of a function template, after its header, to the end of its body or ';': fills the
// tokens, the signature and whether it has a body, and returns the name. A member also takes "const" after
// its parameters.
std::string Parser::readTemplateFunction(FunctionTemplate& declaration, size_t headerStart, bool member,
	std::pair<size_t, size_t>* explicitArguments) {
	// The declarator, up to the parameter list. The name is the identifier right before
	// the first "(" outside angle brackets, and only a group that is followed by what a
	// function can have after its parameters is taken as the list. Attributes are
	// skipped whole, so their own parentheses are never mistaken for it. Any other shape
	// (decltype or typeof in the return type, a parenthesised name) cannot be told apart
	// from it by a rule, so it is refused: a wrong name would let a helper take the call.
	const size_t declaratorStart = _position;
	std::string name;
	size_t angles = 0;
	// An explicit specialization may name its arguments after the name, "f<int>(...)": the name before a
	// "<" at the outermost level, and that group when the parameter list follows it.
	std::string groupName;
	size_t groupStart = 0;
	size_t groupEnd = 0;
	for (;;) {
		if (at(TokenKind::EndOfFile) || at(TokenKind::Semicolon) || at(TokenKind::LBrace)
			|| at(TokenKind::Assign)) {
			throw CompileError("a template declaration that is not a function is not lowered");
		}
		if (at(TokenKind::LParen)) {
			if (angles == 0) {
				if (explicitArguments && !groupName.empty() && name.empty() && groupEnd + 1 == _position) {
					name = groupName;
					*explicitArguments = { groupStart, groupEnd };
				}
				break;
			}
			// A "(" inside the return type's own angle brackets, as in enable_if<(N > 2)>.
			skipBalanced(TokenKind::LParen, TokenKind::RParen);
			continue;
		}
		if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
			skipBalanced(TokenKind::LBracket, TokenKind::RBracket);
			continue;
		}
		if (atKeyword("__attribute__") && lookahead().kind == TokenKind::LParen) {
			advance();
			skipBalanced(TokenKind::LParen, TokenKind::RParen);
			continue;
		}
		if (atKeyword("operator")) {
			throw CompileError("an operator template is not lowered");
		}
		if (angles == 0 && !member && kind() == TokenKind::Identifier && declaration.stage == Stage::None
			&& stageForKeyword(current().text) != Stage::None) {
			throw CompileError("an attribute or specifier before \"" + std::string(current().text)
				+ "\" in an entry-point template is not lowered");
		}
		if (at(TokenKind::Less)) {
			if (angles == 0) {
				groupName = name;
				groupStart = _position;
			}
			++angles;
		} else if (at(TokenKind::Greater) && angles > 0) {
			--angles;
			groupEnd = angles == 0 ? _position : groupEnd;
		} else if (at(TokenKind::ShiftRight) && angles > 1) {
			angles -= 2;
			groupEnd = angles == 0 ? _position : groupEnd;
		}
		name = kind() == TokenKind::Identifier ? std::string(current().text) : std::string();
		advance();
	}
	if (name.empty()) {
		throw CompileError("a template declaration that is not a function is not lowered");
	}
	const auto notNameable = [&] {
		return CompileError("the declarator of the template \"" + name + "\" is not a shape mslc can name "
			"the function in (a decltype or typeof return type, or a parenthesised name); it is not lowered");
	};
	// Defence in depth: the follower and second-group checks below already refuse these.
	if (name == "decltype" || name == "__typeof__" || name == "typeof" || name == "sizeof"
		|| name == "alignof" || name == "noexcept") {
		throw notNameable();
	}
	if (declaration.constructorMember && (lookahead().kind == TokenKind::RParen
			|| (lookahead().kind == TokenKind::Identifier && lookahead().text == "void"
				&& lookahead(2).kind == TokenKind::RParen))) {
		throw CompileError("constructor template \"" + name + "\" takes no arguments, which is not lowered yet");
	}

	skipBalanced(TokenKind::LParen, TokenKind::RParen);
	if (!(at(TokenKind::EndOfFile) || at(TokenKind::LBrace) || at(TokenKind::Semicolon) || at(TokenKind::Assign)
		|| at(TokenKind::Arrow) || atKeyword("__attribute__") || (member && atKeyword("const"))
		|| (declaration.constructorMember && at(TokenKind::Colon)))) {
		throw notNameable();
	}
	// To the body or ';'. A real parameter list is never followed by a second group
	// before a trailing return type, so one means the first was a decltype or a name.
	if (declaration.constructorMember && at(TokenKind::Colon)) {
		advance();
		while (kind() == TokenKind::Identifier
			&& (lookahead().kind == TokenKind::LParen || lookahead().kind == TokenKind::LBrace)) {
			advance();
			if (at(TokenKind::LParen)) {
				skipBalanced(TokenKind::LParen, TokenKind::RParen);
			} else {
				skipBalanced(TokenKind::LBrace, TokenKind::RBrace);
			}
			if (!match(TokenKind::Comma)) {
				break;
			}
		}
	}
	while (!at(TokenKind::LBrace) && !at(TokenKind::Semicolon)) {
		if (at(TokenKind::EndOfFile)) {
			throw CompileError("expected a body or ';' after the template function \"" + name + "\"");
		}
		if (at(TokenKind::Arrow)) {
			while (!at(TokenKind::LBrace) && !at(TokenKind::Semicolon) && !at(TokenKind::EndOfFile)) {
				advance();
			}
			continue;
		}
		if (atKeyword("__attribute__") && lookahead().kind == TokenKind::LParen) {
			advance();
			skipBalanced(TokenKind::LParen, TokenKind::RParen);
		} else if (at(TokenKind::LParen)) {
			throw notNameable();
		} else {
			advance();
		}
	}
	const size_t signatureEnd = _position;
	declaration.hasBody = at(TokenKind::LBrace);
	if (declaration.hasBody) {
		skipBalanced(TokenKind::LBrace, TokenKind::RBrace);
	} else {
		advance();
	}

	for (size_t i = headerStart; i < signatureEnd; ++i) {
		declaration.signature += std::string(_tokens[i].text) + " ";
	}
	declaration.tokens.assign(_tokens.begin() + declaratorStart, _tokens.begin() + _position);
	declaration.tokens.emplace_back();
	return name;
}

// "template RET name<arguments>(parameters);" of a helper function template, or
// "template [[host_name("...")]] stage RET name<arguments>(parameters);" of an entry point.
// Entry-point instantiations are queued and lowered into _unit.functions at the end of parse().
void Parser::parseExplicitInstantiation() {
	const size_t startLine = line();
	std::optional<std::string> hostName;
	while (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
		advance(); advance();
		if (!at(TokenKind::RBracket)) {
			do {
				if (kind() != TokenKind::Identifier) { throw CompileError("expected an attribute name in an explicit template instantiation"); }
				const std::string attr(advance().text);
				if (attr != "host_name") { throw CompileError("attribute \"" + attr + "\" on an explicit template instantiation is not lowered"); }
				if (hostName) { throw CompileError("attribute \"host_name\" cannot appear more than once"); }
				expect(TokenKind::LParen, "after [[host_name");
				if (!at(TokenKind::StringLiteral)) { throw CompileError("[[host_name]] needs a string literal argument"); }
				std::string value;
				while (at(TokenKind::StringLiteral)) {
					const std::string_view raw = advance().text, inner = raw.substr(1, raw.size() - 2);
					if (inner.find('\\') != std::string_view::npos) { throw CompileError("escape sequences in [[host_name]] are not lowered"); }
					value.append(inner);
				}
				expect(TokenKind::RParen, "to close [[host_name]]");
				if (value.empty()) { throw CompileError("[[host_name]] cannot be empty"); }
				hostName = std::move(value);
			} while (match(TokenKind::Comma));
		}
		expect(TokenKind::RBracket, "to close an attribute list"); expect(TokenKind::RBracket, "to close an attribute list");
	}
	const Stage stage = kind() == TokenKind::Identifier ? stageForKeyword(current().text) : Stage::None;
	if (stage != Stage::None) {
		advance();
		if (!hostName) {
			throw CompileError(at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket
				? "an attribute list cannot appear after the stage keyword; write [[host_name(\"...\")]] before it"
				: "an explicit template instantiation of an entry point needs [[host_name(\"...\")]]");
		}
		if (atKeyword("inline") || atKeyword("static") || atKeyword("constexpr") || at(TokenKind::LBracket) || atKeyword("auto")) {
			throw CompileError("invalid specifier or attribute in an explicit template instantiation");
		}
		const Type returnType = parseType();
		QualifiedName spelled;
		if (!peekQualifiedName(spelled)) { throw CompileError("expected a function template name in an explicit template instantiation"); }
		for (size_t i = 0; i < spelled.tokens; ++i) { advance(); }
		const std::string spelledText = (spelled.global ? "::" : "") + joinParts(spelled.parts, 0, spelled.parts.size());
		std::vector<std::string> alternatives;
		std::set<std::string> candidateNames { resolveName(spelled, &alternatives) };
		candidateNames.insert(alternatives.begin(), alternatives.end());
		if (std::none_of(candidateNames.begin(), candidateNames.end(),
			[&](const std::string& each) { return _entryTemplateNames.count(each) != 0; })) {
			throw CompileError("an explicit instantiation of \"" + spelledText + "\" does not name an entry-point template");
		}
		std::vector<TemplateArgument> explicitArgs;
		if (match(TokenKind::Less)) {
			if (!at(TokenKind::Greater) && !at(TokenKind::ShiftRight)) {
				do { explicitArgs.push_back(parseTemplateArgument()); } while (match(TokenKind::Comma));
			}
			closeAngle(("to close explicit template arguments of \"" + spelledText + "\"").c_str());
		}
		expect(TokenKind::LParen, "to open the parameters of an explicit template instantiation");
		std::vector<Parameter> instParams;
		{
			const LocalScope parameters(*this);
			parseEntryParameterList(instParams, "explicit instantiation \"" + spelledText + "\"", true);
		}
		if (at(TokenKind::LBrace) || at(TokenKind::LBracket) || at(TokenKind::Arrow)) {
			throw CompileError(at(TokenKind::LBrace) ? "an explicit template instantiation cannot have a definition"
				: "an attribute or trailing return type after an explicit template instantiation is not lowered");
		}
		expect(TokenKind::Semicolon, "after an explicit template instantiation");
		_pendingEntryInstantiations.push_back({ _unit.functions.size(), startLine, stage, *hostName,
			spelledText, std::move(candidateNames), returnType, std::move(explicitArgs), std::move(instParams) });
		_unit.functions.push_back({ stage, {}, std::move(*hostName), {}, {}, startLine });
		return;
	}
	if (hostName) { throw CompileError("[[host_name]] is only supported on an explicit instantiation of an entry point"); }

	size_t open = 0;
	for (; lookahead(open).kind != TokenKind::LParen; ++open) {
		const Token& token = lookahead(open);
		if (token.kind == TokenKind::EndOfFile || token.kind == TokenKind::Semicolon) {
			throw CompileError("an explicit template instantiation is not lowered");
		}
		if (token.kind == TokenKind::LBracket || (token.kind == TokenKind::Identifier
			&& (token.text == "kernel" || token.text == "vertex" || token.text == "fragment"))) {
			throw CompileError("an explicit template instantiation of an entry point is not lowered");
		}
	}
	size_t name = open;
	if (name > 0 && lookahead(name - 1).kind == TokenKind::Greater) {
		int depth = 0;
		do {
			--name;
			depth += lookahead(name).kind == TokenKind::Greater ? 1 : lookahead(name).kind == TokenKind::Less ? -1 : 0;
		} while (depth > 0 && name > 0);
	}
	if (name == 0 || lookahead(name - 1).kind != TokenKind::Identifier
		|| (name > 1 && lookahead(name - 2).kind == TokenKind::ColonColon)) {
		throw CompileError("an explicit template instantiation is not lowered");
	}
	const std::string spelled(lookahead(name - 1).text);
	if (!_unit.functionTemplates.count(qualify(spelled)) && !_unit.functionTemplates.count(spelled)) {
		if (_entryTemplateNames.count(qualify(spelled)) || _entryTemplateNames.count(spelled)) {
			throw CompileError("an explicit template instantiation of entry-point template \"" + spelled
				+ "\" needs its stage keyword and [[host_name(\"...\")]]");
		}
		throw CompileError("an explicit instantiation of \"" + spelled + "\" does not name a function template");
	}
	while (!at(TokenKind::Semicolon)) {
		if (at(TokenKind::EndOfFile)) {
			throw CompileError("an explicit template instantiation is not terminated");
		}
		advance();
	}
	advance();
}

// "template" at file scope: a function template. It is parsed far enough to know its parameters,
// its name and where it ends; the tokens and the name state are kept, and a call that selects it
// has it re-parsed with its arguments bound (instantiateTemplate). A body is only meaningful with
// arguments (a dependent name has no type until then), so one that is never instantiated needs
// nothing more, as in clang.
void Parser::parseTemplateDeclaration() {
	advance();
	if (!at(TokenKind::Less)) {
		parseExplicitInstantiation();
		return;
	}
	if (lookahead().kind == TokenKind::Greater) {
		parseExplicitSpecialization(_position);
		return;
	}

	FunctionTemplate declaration;
	declaration.line = line();
	const size_t headerStart = _position;
	parseTemplateHeader(declaration);

	if (atKeyword("struct") || atKeyword("class") || atKeyword("union")) {
		ClassTemplate classDeclaration;
		classDeclaration.line = declaration.line;
		classDeclaration.parameters = std::move(declaration.parameters);
		parseClassTemplate(classDeclaration);
		return;
	}
	if (atKeyword("using") || atKeyword("template")) {
		throw CompileError("an alias template is not lowered");
	}

	declaration.stage = kind() == TokenKind::Identifier ? stageForKeyword(current().text) : Stage::None;
	if (declaration.stage != Stage::None
		&& std::find(_namespacePath.begin(), _namespacePath.end(), kAnonymousNamespace) != _namespacePath.end()) {
		throw CompileError(std::string(current().text) + " function cannot be declared in anonymous namespace");
	}

	const std::string name = readTemplateFunction(declaration, headerStart, false);

	const std::string qualified = qualify(name);
	declaration.name = qualified;
	declaration.order = declaration.bodyOrder = _functionOrder++;
	declaration.globalsBefore = _unit.globals.size();
	if (const auto kinds = kindsOf(qualified); !kinds.empty() && std::any_of(kinds.begin(), kinds.end(),
		[](std::string_view kindName) { return kindName != "function"; })) {
		throw CompileError("the function template \"" + qualified + "\" shares its name with a type or a "
			"variable, which is not lowered: a call spelled like a construction would not reach it");
	}
	declareName(qualified, "function");
	if (declaration.stage != Stage::None) { rejectUsingConflict(qualified, "function", nullptr); }
	(declaration.stage == Stage::None ? _unit.functionTemplates : _entryTemplateNames).insert(qualified);
	std::vector<FunctionTemplate>& targetList = declaration.stage == Stage::None
		? _unit.templates : _unit.entryTemplates;
	declaration.endAt = _position;

	// A prototype and its definition are one template, found at the prototype's place in the file.
	for (FunctionTemplate& earlier: targetList) {
		if (earlier.name != qualified || earlier.signature != declaration.signature) {
			continue;
		}
		if (earlier.hasBody && declaration.hasBody) {
			throw CompileError("redefinition of the function template \"" + qualified + "\"");
		}
		if (declaration.hasBody) {
			declaration.order = earlier.order;
			earlier = std::move(declaration);
		}
		return;
	}
	targetList.push_back(std::move(declaration));
}

// "template <>" and a function: an explicit specialization of a function template declared above. It is
// kept as a template of no parameters, the explicit arguments taken out of its declarator, for sema to
// match against the primary templates of its name.
void Parser::parseExplicitSpecialization(size_t headerStart) {
	advance();
	advance();
	if (atKeyword("struct") || atKeyword("class") || atKeyword("union")) {
		throw CompileError("an explicit specialization of a class template is not lowered; Apple accepts it, and "
			"mslc refuses it so that the primary template is never silently used in its place");
	}
	if (atKeyword("template")) {
		throw CompileError("an explicit specialization of a member template is not lowered");
	}
	ExplicitSpecialization specialization;
	FunctionTemplate& declaration = specialization.declaration;
	declaration.line = line();
	std::pair<size_t, size_t> group { 0, 0 };
	const size_t declaratorStart = _position;
	const std::string name = readTemplateFunction(declaration, headerStart, false, &group);
	const std::string qualified = qualify(name);
	if (!_unit.functionTemplates.count(qualified)) {
		throw CompileError("\"" + qualified + "\" is explicitly specialized, and no function template of that name "
			"is declared above it");
	}
	if (!declaration.hasBody) {
		throw CompileError("the explicit specialization of \"" + qualified + "\" is declared and not defined here; "
			"a declaration without a body is not lowered");
	}
	const size_t end = _position;
	if (group.second) {
		_position = group.first + 1;
		const size_t before = _tokens.size();
		while (!at(TokenKind::Greater) && !at(TokenKind::ShiftRight)) {
			specialization.given.push_back(parseTemplateArgument());
			if (!match(TokenKind::Comma)) {
				break;
			}
		}
		// closeAngle splits a ">>" that closes the last argument and this list into two tokens.
		const size_t inserted = _tokens.size() - before;
		if (_position != group.second + inserted || (!at(TokenKind::Greater) && !at(TokenKind::ShiftRight))) {
			throw CompileError("the template arguments of the explicit specialization of \"" + qualified
				+ "\" are not lowered: expected \">\" where \"" + std::string(current().text) + "\" is");
		}
		_position = end + inserted;
		// The tokens without the arguments, so the declarator reads as a function's.
		const size_t from = group.first - declaratorStart;
		const size_t to = group.second + 1 - declaratorStart;
		declaration.tokens.erase(declaration.tokens.begin() + from, declaration.tokens.begin() + to);
	}
	declaration.name = qualified;
	declaration.order = declaration.bodyOrder = _functionOrder++;
	declaration.globalsBefore = _unit.globals.size();
	declaration.endAt = _position;
	_unit.specializations.push_back(std::move(specialization));
}

// Binds the template's parameters in the innermost block: a type is a typedef, a constant an
// enumerator-like value, each under a key no spelling in the source can reach, and the
// parameter's own name is mapped to it as a using declaration is, so a local of that name in a
// nested block still hides it.
void Parser::bindTemplateArguments(const FunctionTemplate& declaration, const std::vector<TemplateArgument>& arguments) {
	bindParameters(declaration.name, declaration.parameters, arguments);
}

void Parser::bindParameters(const std::string& owner, const std::vector<TemplateParameter>& parameters,
	const std::vector<TemplateArgument>& arguments) {
	if (arguments.size() != parameters.size()) {
		throw CompileError("the template \"" + owner + "\" takes " + std::to_string(parameters.size())
			+ " arguments, and " + std::to_string(arguments.size()) + " are bound");
	}
	for (size_t i = 0; i < arguments.size(); ++i) {
		const TemplateParameter& parameter = parameters[i];
		const TemplateArgument& argument = arguments[i];
		if (argument.isType != parameter.isType) {
			throw CompileError("the template argument for \"" + parameter.name + "\" of \"" + owner
				+ "\" has to be " + (parameter.isType ? "a type" : "a constant"));
		}
		if (parameter.name.empty()) {
			continue;
		}
		const std::string key = "<template " + parameter.name + ">";
		if (parameter.isType) {
			_typedefs[key] = argument.type;
		} else if (!argument.unbound) {
			_enumConstants[key] = EnumConstant { argument.value, parameter.kind, std::string() };
		}
		_blocks.back().names[parameter.name] = key;
	}
}

namespace {

// A failure to read a class template instance. It is not swallowed where the parser backtracks, since
// reading on past it would leave a half-declared struct behind.
class ClassTemplateError: public CompileError {
public:
	using CompileError::CompileError;
};

// An instance of a class template whose arguments are a function template's parameters still to be deduced.
class DependentClassInstance: public ClassTemplateError {
public:
	using ClassTemplateError::ClassTemplateError;
};

bool isDeductionMarker(const Type& type);

}

// One instance of a template as a helper. The declaration's tokens are the whole input.
FunctionDecl Parser::parseInstance(const FunctionTemplate& declaration, const std::vector<TemplateArgument>& arguments,
	bool withBody) {
	restoreNames(*namesAt(declaration));
	const LocalScope templateScope(*this);
	_inInstance = true;
	const bool member = !declaration.memberOf.empty();
	if (member) {
		_memberFields = &declaration.memberFields;
		_memberFunctions = declaration.memberFunctions;
		_memberStruct = declaration.memberOf;
		if (const ClassTemplate* owner = declaration.ownerTemplate.get()) {
			_classMemberInstance = true;
			_definedNames = &owner->declaredBefore;
			bindParameters(owner->name, owner->parameters, declaration.ownerArguments);
			for (const TemplateParameter& parameter: declaration.parameters) {
				for (const TemplateParameter& outer: owner->parameters) {
					if (!parameter.name.empty() && parameter.name == outer.name) {
						throw CompileError("the template parameter \"" + parameter.name + "\" of the member template \""
							+ declaration.name + "\" shadows a parameter of its class template");
					}
				}
			}
		}
	}
	bindTemplateArguments(declaration, arguments);

	const auto skipAttributes = [&] {
		while (atKeyword("__attribute__") && lookahead().kind == TokenKind::LParen) {
			skipFunctionAttribute();
		}
	};
	FunctionDecl decl;
	decl.stage = declaration.stage;
	decl.line = declaration.line;
	decl.name = declaration.name;
	decl.order = declaration.bodyOrder;
	decl.globalsBefore = declaration.globalsBefore;
	const std::string quoted = "function template \"" + declaration.name + "\"";

	if (declaration.stage != Stage::None) {
		advance();
	} else {
		for (;;) {
			skipAttributes();
			if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
				if (skipStandardFunctionAttribute(true)) {
					continue;
				}
				break;
			}
			if (atKeyword("inline") || atKeyword("static") || atKeyword("constexpr")) {
				advance();
				continue;
			}
			break;
		}
	}
	if (at(TokenKind::LBracket)) {
		throw CompileError("an attribute in the declaration of " + quoted + " is not lowered yet");
	}
	if (atKeyword("auto")) {
		throw CompileError("a trailing return type on " + quoted + " is not lowered yet");
	}
	const std::string written = declaration.name.substr(declaration.name.rfind("::") == std::string::npos
		? 0 : declaration.name.rfind("::") + 2);
	// The signature read to deduce the parameters, some of which stand for themselves: a return type that is
	// an instance of a class template on those parameters cannot be made yet, and deduction does not need it.
	// It is a dependent type, as a return type of T is; the instance a call chooses reads the real one.
	const bool deducing = !withBody && std::any_of(arguments.begin(), arguments.end(), [](const TemplateArgument& argument) {
		return argument.unbound || (argument.isType && isDeductionMarker(argument.type));
	});
	const size_t returnTypeStart = _position;
	bool dependentReturn = false;
	if (declaration.constructorMember) {
		decl.returnType.namedType = declaration.memberOf;
	} else {
		try {
			decl.returnType = parseType();
		} catch (const DependentClassInstance&) {
			if (!deducing) {
				throw;
			}
			_position = returnTypeStart;
			// A "<" or ">" inside parentheses, "(1 < 2)", is an operator, not a bracket.
			size_t angles = 0;
			size_t parentheses = 0;
			while (!at(TokenKind::EndOfFile) && !(angles == 0 && parentheses == 0 && kind() == TokenKind::Identifier
				&& current().text == written && lookahead().kind == TokenKind::LParen)) {
				parentheses += at(TokenKind::LParen) ? 1 : 0;
				parentheses -= at(TokenKind::RParen) && parentheses > 0 ? 1 : 0;
				if (parentheses == 0) {
					angles += at(TokenKind::Less) ? 1 : 0;
					angles -= at(TokenKind::Greater) && angles > 0 ? 1 : 0;
					angles -= at(TokenKind::ShiftRight) && angles > 1 ? 2 : 0;
				}
				advance();
			}
			if (at(TokenKind::EndOfFile)) {
				throw;
			}
			decl.returnType = Type();
			decl.returnType.namedType = "<dependent return type>";
			dependentReturn = true;
		}
	}
	if (declaration.stage == Stage::None) {
		rejectUnloweredReturn(decl.returnType, quoted);
	}
	if (kind() != TokenKind::Identifier || current().text != written) {
		throw CompileError("the declaration of " + quoted + " is not a return type and a name that mslc reads "
			"when it is instantiated (found \"" + std::string(current().text) + "\" where its name is)");
	}
	advance();
	if (at(TokenKind::LBracket)) {
		throw CompileError("an attribute in the declaration of " + quoted + " is not lowered yet");
	}
	expect(TokenKind::LParen, "to open the parameters of a function template");

	const LocalScope parameters(*this);
	_parameterBlock = _blocks.size() - 1;
	// The object is a first parameter, as in a member function.
	const bool object = member && !declaration.staticMember && !declaration.constructorMember;
	const size_t objectParameters = object ? 1 : 0;
	if (object || declaration.constructorMember) {
		declareLocal("this");
	}
	if (object) {
		Parameter self;
		self.type.namedType = declaration.memberOf;
		self.name = "this";
		decl.parameters.push_back(std::move(self));
	}
	const size_t parametersFrom = _position;
	if (declaration.stage != Stage::None) {
		parseEntryParameterList(decl.parameters, quoted, !withBody && !declaration.hasBody);
		if (withBody) {
			checkUniqueResourceSlots(decl);
		}
	} else {
		parseHelperParameters(decl, quoted);
	}
	// A parameter is dependent when the tokens that declare it name a template parameter.
	{
		size_t index = 0;
		bool dependent = false;
		int depth = 0;
		for (size_t i = parametersFrom; i + 1 < _position; ++i) {
			const Token& token = _tokens[i];
			if (token.kind == TokenKind::LParen || token.kind == TokenKind::LBracket || token.kind == TokenKind::Less) {
				++depth;
			} else if (token.kind == TokenKind::RParen || token.kind == TokenKind::RBracket || token.kind == TokenKind::Greater) {
				--depth;
			}
			if (token.kind == TokenKind::Comma && depth == 0) {
				if (dependent && index + objectParameters < decl.parameters.size()) {
					_dependentParameters.insert(decl.parameters[index + objectParameters].name);
				}
				++index;
				dependent = false;
			} else if (token.kind == TokenKind::Identifier) {
				for (const TemplateParameter& parameter: declaration.parameters) {
					dependent = dependent || (!parameter.name.empty() && parameter.name == token.text);
				}
			}
		}
		if (dependent && index + objectParameters < decl.parameters.size()) {
			_dependentParameters.insert(decl.parameters[index + objectParameters].name);
		}
	}
	if (object && !matchIdentifier("const")) {
		throw CompileError(quoted + " is not const; mslc takes the object by value, so a member function that "
			"could write it is not lowered (mark it const)");
	}
	if (declaration.constructorMember && matchIdentifier("const")) {
		throw CompileError(quoted + " is a constructor and cannot be declared const");
	}
	// A struct the template's own tokens name has to be declared above it; one an argument brings is not
	// looked up here (sema sees every struct, so it cannot tell the two apart).
	std::set<std::string> bound;
	for (const TemplateArgument& argument: arguments) {
		bound.insert(argument.type.namedType);
	}
	const auto declared = [&](const Type& type) {
		if (!type.namedType.empty() && !_unit.findStruct(type.namedType) && !bound.count(type.namedType)
			&& !_dependentInstances.count(type.namedType)) {
			throw CompileError("unknown type name \"" + type.namedType + "\" in the declaration of " + quoted);
		}
	};
	if (!dependentReturn) {
		declared(decl.returnType);
	}
	for (const Parameter& parameter: decl.parameters) {
		declared(parameter.type);
	}
	decl.dependentInstances = _dependentInstances;
	skipAttributes();
	if (at(TokenKind::LBracket)) {
		throw CompileError("attributes on " + quoted + " are not lowered yet");
	}
	if (at(TokenKind::Arrow)) {
		throw CompileError("a trailing return type on " + quoted + " is not lowered yet");
	}
	if (declaration.constructorMember && decl.parameters.empty()) {
		throw CompileError("constructor template \"" + declaration.name + "\" takes no arguments, which is not lowered yet");
	}
	if (!withBody || (!declaration.constructorMember && match(TokenKind::Semicolon))) {
		return decl;
	}
	std::vector<std::pair<std::string, ExpressionPtr>> initialisers;
	if (declaration.constructorMember && match(TokenKind::Colon)) {
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
			ExpressionPtr value = parseAssignment();
			expect(TokenKind::RParen, "to close a member initialiser");
			initialisers.emplace_back(field, std::move(value));
		} while (match(TokenKind::Comma));
	}
	decl.body = parseCompoundStatement(true);
	if (!at(TokenKind::EndOfFile)) {
		throw CompileError("unexpected \"" + std::string(current().text) + "\" after the body of " + quoted);
	}
	if (declaration.constructorMember) {
		const auto rewriteReturn = [&](const auto& self, Statement& s) -> void {
			if (s.kind == StatementKind::Return) {
				if (s.expression) {
					throw CompileError(quoted + " is a constructor and cannot return a value");
				}
				s.expression = std::make_unique<Expression>();
				s.expression->kind = ExpressionKind::Identifier;
				s.expression->line = s.line;
				s.expression->name = "this";
			}
			for (StatementPtr& child: s.children) { self(self, *child); }
			if (s.thenBranch) { self(self, *s.thenBranch); }
			if (s.elseBranch) { self(self, *s.elseBranch); }
			if (s.forBody) { self(self, *s.forBody); }
			if (s.whileBody) { self(self, *s.whileBody); }
			for (SwitchCase& sc: s.switchCases) {
				for (StatementPtr& child: sc.body) { self(self, *child); }
			}
			for (StatementPtr& child: s.switchPreamble) { self(self, *child); }
		};
		rewriteReturn(rewriteReturn, *decl.body);
		auto wrapper = std::make_unique<Statement>();
		wrapper->kind = StatementKind::Compound;
		wrapper->line = decl.line;
		auto selfLocal = std::make_unique<Statement>();
		selfLocal->kind = StatementKind::DeclarationStatement;
		selfLocal->line = decl.line;
		selfLocal->declaration = VariableDeclaration();
		selfLocal->declaration->type = decl.returnType;
		selfLocal->declaration->name = "this";
		wrapper->children.push_back(std::move(selfLocal));
		for (const std::string& fieldName: declaration.memberFieldOrder) {
			for (auto& [name, value]: initialisers) {
				if (name != fieldName) {
					continue;
				}
				auto assign = std::make_unique<Expression>();
				assign->kind = ExpressionKind::Assign;
				assign->line = decl.line;
				assign->left = std::make_unique<Expression>();
				assign->left->kind = ExpressionKind::Member;
				assign->left->line = decl.line;
				assign->left->left = std::make_unique<Expression>();
				assign->left->left->kind = ExpressionKind::Identifier;
				assign->left->left->line = decl.line;
				assign->left->left->name = "this";
				assign->left->memberName = name;
				assign->left->height = 2;
				assign->right = std::move(value);
				measure(*assign);
				auto statement = std::make_unique<Statement>();
				statement->kind = StatementKind::ExpressionStatement;
				statement->line = decl.line;
				statement->expression = std::move(assign);
				wrapper->children.push_back(std::move(statement));
			}
		}
		wrapper->children.push_back(std::move(decl.body));
		auto result = std::make_unique<Statement>();
		result->kind = StatementKind::Return;
		result->line = decl.line;
		result->expression = std::make_unique<Expression>();
		result->expression->kind = ExpressionKind::Identifier;
		result->expression->line = decl.line;
		result->expression->name = "this";
		wrapper->children.push_back(std::move(result));
		decl.body = std::move(wrapper);
	}
	return decl;
}

FunctionDecl instantiateTemplate(const FunctionTemplate& declaration,
	const std::vector<TemplateArgument>& arguments, bool withBody) {
	Parser parser(declaration.tokens);
	try {
		return parser.parseInstance(declaration, arguments, withBody);
	} catch (const SubstitutionFailure& failure) {
		// Only the signature is a substitution: a failure in the body, which a chosen instance reads, is an error.
		if (!withBody) {
			throw;
		}
		throw CompileError("in the function template \"" + declaration.name + "\" (declared at line "
			+ std::to_string(declaration.line) + "), which a call instantiates: " + failure.what());
	} catch (const CompileError& error) {
		throw CompileError("in the function template \"" + declaration.name + "\" (declared at line "
			+ std::to_string(declaration.line) + "), which a call instantiates: " + error.what());
	}
}

TemplateArgument templateDefault(const FunctionTemplate& declaration, size_t index,
	const std::vector<TemplateArgument>& bound) {
	const TemplateParameter& parameter = declaration.parameters.at(index);
	const std::string what = "the default argument of the template parameter \"" + parameter.name + "\" of \""
		+ declaration.name + "\"";
	Parser parser(parameter.defaultTokens);
	parser.restoreNames(*Parser::namesAt(declaration));
	const Parser::LocalScope scope(parser);
	if (const ClassTemplate* owner = declaration.ownerTemplate.get()) {
		parser._definedNames = &owner->declaredBefore;
		parser.bindParameters(owner->name, owner->parameters, declaration.ownerArguments);
	}
	// Only the parameters before this one are in scope.
	FunctionTemplate earlier;
	earlier.name = declaration.name;
	earlier.parameters.assign(declaration.parameters.begin(), declaration.parameters.begin() + index);
	parser.bindTemplateArguments(earlier, std::vector<TemplateArgument>(bound.begin(), bound.begin() + index));
	TemplateArgument result;
	result.isType = parameter.isType;
	try {
		if (parameter.isType) {
			result.type = parser.parseType();
			if (result.type.isPointer || result.type.arrayLength || result.type.resource != ResourceKind::None
				|| !result.type.enumName.empty() || result.type.addressSpace != AddressSpace::None
				|| result.type.isVolatile) {
				throw CompileError("that is a pointer, an array, a resource, an enum, volatile or in an address space");
			}
		} else {
			const ExpressionPtr expression = parser.parseAssignment();
			result.value = expression->kind == ExpressionKind::BoolLiteral ? (expression->boolValue ? 1 : 0)
				: parser.evaluateConstant(*expression, false);
		}
		if (!parser.at(TokenKind::EndOfFile)) {
			throw CompileError("unexpected \"" + std::string(parser.current().text) + "\"");
		}
	} catch (const SubstitutionFailure&) {
		throw;
	} catch (const CompileError& error) {
		throw CompileError(what + " is not lowered: " + error.what());
	}
	return result;
}

// One explicit template argument: a type, or an integer constant that stops at the "," or ">"
// that ends it (a larger expression needs parentheses, as in C++).
TemplateArgument Parser::parseTemplateArgument() {
	TemplateArgument argument;
	Type probe;
	size_t nameTokens = 0;
	const bool named = kind() == TokenKind::Identifier || at(TokenKind::ColonColon);
	const std::string name = named ? peekResolved(nameTokens) : std::string();
	const bool localHides = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon && isLocal(name);
	size_t vecTokens = 0;
	const bool isTypeArgument = (named && (resolveTypeName(name, probe) || isTypeQualifier(name)
			|| (_unit.findStruct(name) != nullptr && !localHides)))
		|| (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text)) || atLibraryVec(vecTokens);
	if (isTypeArgument) {
		argument.type = parseType();
		if (argument.type.isPointer || argument.type.arrayLength || argument.type.resource != ResourceKind::None
			|| !argument.type.enumName.empty() || argument.type.addressSpace != AddressSpace::None
			|| argument.type.isVolatile) {
			throw CompileError("a template argument that is a pointer, an array, a resource, an enum, volatile or "
				"in an address space is not lowered");
		}
		return argument;
	}
	argument.isType = false;
	const ExpressionPtr value = parseAdditive();
	if (!at(TokenKind::Comma) && !at(TokenKind::Greater)) {
		throw CompileError("this template argument is not lowered: a constant that is more than an arithmetic "
			"expression needs parentheses");
	}
	argument.value = value->kind == ExpressionKind::BoolLiteral ? (value->boolValue ? 1 : 0)
		: evaluateConstant(*value, false);
	return argument;
}

namespace {

// A trait's argument is a deduction marker: its value is not known until the type is.
class DependentTrait: public CompileError {
public:
	using CompileError::CompileError;
};

// Sets a parser flag for a scope and puts it back on every way out, an exception included: a
// failed read is retried (tryParseHelperFunction backtracks) with the parser's flags as they were.
class RestoreFlag {
	bool& _flag;
	bool _old;

public:
	RestoreFlag(bool& flag, bool value): _flag(flag), _old(flag) { _flag = value; }
	~RestoreFlag() { _flag = _old; }
};

bool isDeductionMarker(const Type& type) {
	return type.namedType.size() > 2 && type.namedType.front() == '<' && type.namedType.back() == '>';
}

bool isScalarOrVector(const Type& type) {
	return type.matrixColumns == 0 && type.namedType.empty() && type.enumName.empty() && type.scalar != ScalarKind::Void;
}

bool sameTraitType(const Type& a, const Type& b) {
	return a.scalar == b.scalar && a.vectorWidth == b.vectorWidth && a.matrixColumns == b.matrixColumns
		&& a.isPacked == b.isPacked && a.namedType == b.namedType && a.enumName == b.enumName && a.isConst == b.isConst;
}

}

// <type_traits> as Metal's library has it (measured against Apple's compiler), for the types mslc
// has: scalar, vector (packed or not), matrix, struct and enum. A vector counts as floating point,
// integral, signed or unsigned by its element, and as arithmetic; a matrix is a class; an enum is
// a scalar and nothing else numeric. A type with a pointer, array, address space or resource is
// refused, not guessed at.
static bool traitHolds(const std::string& trait, const std::vector<Type>& operands, const TranslationUnit& unit) {
	for (const Type& type: operands) {
		if (type.isVolatile) {
			throw CompileError("the trait \"" + trait + "\" of a volatile type is not lowered");
		}
		if (isDeductionMarker(type)) {
			throw DependentTrait("the trait \"" + trait + "\" depends on a type that is not deduced yet");
		}
		if (type.isPointer || type.arrayLength || type.resource != ResourceKind::None || type.isAtomic
			|| type.addressSpace != AddressSpace::None || type.isAuto) {
			throw CompileError("the trait \"" + trait + "\" of a pointer, array, texture, sampler, atomic or "
				"address-space-qualified type is not lowered");
		}
		if (!type.namedType.empty() && !unit.findStruct(type.namedType)) {
			throw CompileError("the trait \"" + trait + "\" of \"" + type.namedType + "\", which is not a type "
				"mslc knows");
		}
		if (type.scalar == ScalarKind::Double && isScalarOrVector(type)) {
			throw CompileError("the trait \"" + trait + "\" of a double is not lowered");
		}
	}
	if (trait == "is_same") {
		return sameTraitType(operands.at(0), operands.at(1));
	}
	const Type& type = operands.at(0);
	const bool numeric = isScalarOrVector(type);
	// Apple (measured): a const vector is neither signed nor unsigned, though it is still a vector,
	// floating point or integral and arithmetic.
	const bool constVector = numeric && type.isConst && type.vectorWidth != 0;
	const ScalarKind kind = type.scalar;
	const bool floating = kind == ScalarKind::Half || kind == ScalarKind::Float;
	const bool unsignedKind = kind == ScalarKind::Bool || kind == ScalarKind::UChar || kind == ScalarKind::UShort
		|| kind == ScalarKind::UInt || kind == ScalarKind::ULong;
	if (trait == "is_floating_point") {
		return numeric && floating;
	}
	if (trait == "is_integral") {
		return numeric && !floating;
	}
	if (trait == "is_arithmetic") {
		return numeric;
	}
	if (trait == "is_signed") {
		return numeric && !unsignedKind && !constVector;
	}
	if (trait == "is_unsigned") {
		return numeric && unsignedKind && !constVector;
	}
	if (trait == "is_scalar") {
		return (numeric && type.vectorWidth == 0) || !type.enumName.empty();
	}
	if (trait == "is_vector") {
		return numeric && type.vectorWidth != 0;
	}
	if (trait == "is_class") {
		return type.matrixColumns != 0 || !type.namedType.empty();
	}
	if (trait == "is_enum") {
		return !type.enumName.empty();
	}
	if (trait == "is_const") {
		return type.isConst;
	}
	throw CompileError("the trait \"" + trait + "\" is not lowered");
}

// The ">" that closes a template argument list; a ">>" is two of them, and the first is taken.
void Parser::closeAngle(const char* what) {
	if (at(TokenKind::ShiftRight)) {
		_tokens[_position].kind = TokenKind::Greater;
		_tokens[_position].text = ">";
		_tokens.insert(_tokens.begin() + _position, _tokens[_position]);
	}
	expect(TokenKind::Greater, what);
}

void Parser::reserveSplits(std::vector<Token>& tokens) {
	tokens.reserve(tokens.size() + std::count_if(tokens.begin(), tokens.end(),
		[](const Token& token) { return token.kind == TokenKind::ShiftRight; }));
}

size_t Parser::usingCount(const std::vector<std::string>& path, const std::set<std::string>& mentioned,
	const std::vector<std::string>& aliasTargets) const {
	size_t count = 0;
	std::vector<std::string> work;
	std::set<std::string> seen;
	std::string scope;
	for (size_t i = 0; i <= path.size(); ++i) {
		if (i > 0) {
			scope += (scope.empty() ? "" : "::") + path[i - 1];
		}
		work.push_back(scope);
	}
	// A qualified call "N::h(v)" reads N, which may be written anywhere: a namespace the template names is a scope
	// it reads from, so a using declaration made in it later is counted (and refused) too.
	for (const auto& scope: _scopes) {
		const std::string& key = scope.first;
		const size_t cut = key.rfind("::");
		if (!key.empty() && mentioned.count(cut == std::string::npos ? key : key.substr(cut + 2))) {
			work.push_back(key);
		}
	}
	work.insert(work.end(), aliasTargets.begin(), aliasTargets.end());
	// The scopes the template's own chain reads from, and every one those nominate by a directive, transitively.
	while (!work.empty()) {
		const std::string current = work.back();
		work.pop_back();
		if (!seen.insert(current).second) {
			continue;
		}
		const auto entry = _scopes.find(current);
		if (entry == _scopes.end()) {
			continue;
		}
		for (const std::string& directive: entry->second.directives) {
			bool own = false;
			std::string prefix;
			for (size_t n = 0; n < path.size() && !own; ++n) {
				prefix += (prefix.empty() ? "" : "::") + path[n];
				own = directive == prefix;
			}
			count += own ? 0 : 1;
			work.push_back(directive);
			// A directive may be written relative to the scope it is in.
			work.push_back(current.empty() ? directive : current + "::" + directive);
		}
		for (const std::string& inner: entry->second.inlines) {
			work.push_back(inner);
		}
		for (const auto& declaration: entry->second.stdlibDeclarations) {
			count += mentioned.count(declaration.first) ? declaration.second.size() : 0;
		}
		for (const auto& declaration: entry->second.declarations) {
			count += mentioned.count(declaration.first) ? declaration.second.size() : 0;
		}
	}
	return count;
}

size_t Parser::usingDeclarationsOf(const std::string& name) const {
	size_t count = 0;
	for (const auto& scope: _scopes) {
		if (const auto found = scope.second.declarations.find(name); found != scope.second.declarations.end()) {
			count += found->second.size();
		}
		if (const auto found = scope.second.stdlibDeclarations.find(name); found != scope.second.stdlibDeclarations.end()) {
			count += found->second.size();
		}
	}
	return count;
}

void Parser::swapContext(Context& other) {
	std::swap(_tokens, other.tokens);
	std::swap(_position, other.position);
	std::swap(_blocks, other.blocks);
	std::swap(_structBody, other.structBody);
	std::swap(_pendingMembers, other.pendingMembers);
	std::swap(_pendingMemberTemplates, other.pendingMemberTemplates);
	std::swap(_memberFields, other.memberFields);
	std::swap(_memberFunctions, other.memberFunctions);
	std::swap(_memberStruct, other.memberStruct);
	std::swap(_parameterBlock, other.parameterBlock);
	std::swap(_dependentParameters, other.dependentParameters);
	std::swap(_switchDepth, other.switchDepth);
	std::swap(_declaratorTail, other.declaratorTail);
	std::swap(_noGreater, other.noGreater);
	std::swap(_inEnableIf, other.inEnableIf);
	std::swap(_dependentCondition, other.dependentCondition);
	std::swap(_stopAt, other.stopAt);
	std::swap(_namespacePath, other.namespacePath);
}

// "template <...> struct Name { ... };" at file scope. The body is kept as tokens: nothing in it means
// anything until the parameters are bound, so a template that is never named costs nothing, as in clang.
void Parser::parseClassTemplate(ClassTemplate& declaration) {
	if (!atKeyword("struct")) {
		throw CompileError("a class or union template is not lowered; write it as a struct");
	}
	const size_t first = _position;
	advance();
	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected the name of a class template, found " + std::string(tokenKindName(kind())));
	}
	declaration.spelled = std::string(advance().text);
	const std::string quoted = "the class template \"" + declaration.spelled + "\"";
	if (at(TokenKind::Less)) {
		throw CompileError("a partial or explicit specialization of " + quoted + " is not lowered; Apple accepts "
			"it, and mslc refuses it so that the primary template is never silently used in its place");
	}
	if (at(TokenKind::Semicolon)) {
		throw CompileError("a declaration of " + quoted + " without its definition is not lowered");
	}
	if (!at(TokenKind::LBrace)) {
		throw CompileError("expected the body of " + quoted + "; a base list is not lowered");
	}
	skipBalanced(TokenKind::LBrace, TokenKind::RBrace);
	if (!at(TokenKind::Semicolon)) {
		throw CompileError("a declarator after the body of " + quoted + " is not lowered");
	}
	advance();
	declaration.tokens.assign(_tokens.begin() + first, _tokens.begin() + _position);
	declaration.tokens.emplace_back();

	declaration.name = qualify(declaration.spelled);
	declaration.namespacePath = _namespacePath;
	declaration.order = _functionOrder++;
	declaration.globalsBefore = _unit.globals.size();
	for (const auto& entry: _declared) {
		declaration.declaredBefore.insert(entry.first);
	}
	declaration.declaredBefore.insert(declaration.name);
	for (const Token& token: declaration.tokens) {
		if (token.kind == TokenKind::Identifier) {
			declaration.mentioned.insert(std::string(token.text));
		}
	}
	for (const TemplateParameter& parameter: declaration.parameters) {
		for (const Token& token: parameter.defaultTokens) {
			if (token.kind == TokenKind::Identifier) {
				declaration.mentioned.insert(std::string(token.text));
			}
		}
	}
	// A namespace alias the template names: "A::h(v)" with "namespace A = N;" reads N. Which alias a qualified or
	// directive-reached spelling means is not resolved, so every alias that exists at the definition under a name
	// the template mentions counts (one declared below the template cannot be named by it). A same-named alias in
	// an unrelated namespace is refused too (a documented over-refusal).
	for (const auto& alias: _namespaceAliases) {
		const size_t cut = alias.first.rfind("::");
		if (declaration.mentioned.count(cut == std::string::npos ? alias.first : alias.first.substr(cut + 2))) {
			declaration.aliasTargets.push_back(alias.second);
		}
	}
	declaration.usingsBefore = usingCount(_namespacePath, declaration.mentioned, declaration.aliasTargets);
	for (const std::string& name: declaration.mentioned) {
		declaration.usingDeclarationsBefore[name] = usingDeclarationsOf(name);
	}
	declareName(declaration.name, "class template");
	const std::string name = declaration.name;
	_classTemplates[name] = std::make_shared<const ClassTemplate>(std::move(declaration));
}

const ClassTemplate* Parser::classTemplateNamed(const std::vector<std::string>& parts, bool global) const {
	if (_classTemplates.empty()) {
		return nullptr;
	}
	std::string resolved;
	if (parts.size() == 1 && !global) {
		if (memberHidesName(parts[0])) {
			return nullptr;
		}
		if (const std::string* bound = blockName(parts[0]); bound && bound->empty()) {
			return nullptr;
		} else if (bound) {
			resolved = *bound;
		}
	}
	if (resolved.empty()) {
		QualifiedName name;
		name.parts = parts;
		name.global = global;
		try {
			resolved = resolveName(name);
		} catch (const CompileError&) {
			return nullptr;
		}
	}
	const auto found = _classTemplates.find(resolved);
	return found == _classTemplates.end() ? nullptr : found->second.get();
}

// "<" arguments ">" after the name of a class template, the "<" being the current token.
std::vector<TemplateArgument> Parser::parseClassTemplateArguments(const ClassTemplate& declaration) {
	expect(TokenKind::Less, "to open template arguments");
	std::vector<TemplateArgument> arguments;
	if (!at(TokenKind::Greater) && !at(TokenKind::ShiftRight)) {
		do {
			arguments.push_back(parseTemplateArgument());
		} while (match(TokenKind::Comma));
	}
	closeAngle(("to close the template arguments of \"" + declaration.name + "\"").c_str());
	return arguments;
}

// Makes each seed whose class template and argument structs exist and that is not made yet, in order, at the
// start of a declaration. The instance is read as if the program had spelled it here: the same order and
// the same refusals of a name or a using declaration the class template did not see.
void Parser::makeSeeds() {
	for (size_t i = 0; i < _seeds.size(); ++i) {
		if (_seeded[i]) {
			continue;
		}
		const ClassInstanceSeed& seed = _seeds[i];
		const auto found = _classTemplates.find(seed.className);
		if (found == _classTemplates.end()) {
			break;
		}
		bool known = true;
		for (const TemplateArgument& argument: seed.arguments) {
			known = known && (argument.isType ? argument.type.namedType.empty() || _unit.findStruct(argument.type.namedType)
				: true);
		}
		if (!known) {
			break;
		}
		_seeded[i] = true;
		try {
			instantiateClass(*found->second, seed.arguments);
		} catch (const CompileError& error) {
			throw ClassTemplateError(std::string("the instance \"") + seed.instance + "\", named by a function "
				"template: " + error.what());
		}
	}
}

// The struct "Name<arguments>", declared as the template's tokens read with the arguments bound. It is
// an ordinary struct once declared: its fields and its member functions and constructors, which become
// helpers named after it, are read where the template is used, and each distinct argument list is one
// struct. A member function sees what the template's declaration sees: it counts as declared where the
// template is, so a function written after the template is not one it can call.
std::string Parser::instantiateClass(const ClassTemplate& declaration, std::vector<TemplateArgument> arguments) {
	const std::string quoted = "the class template \"" + declaration.name + "\"";
	const std::vector<TemplateParameter>& parameters = declaration.parameters;
	if (arguments.size() > parameters.size()) {
		throw ClassTemplateError(quoted + " takes " + std::to_string(parameters.size()) + " arguments, and "
			+ std::to_string(arguments.size()) + " are given");
	}
	bool dependent = false;
	for (const TemplateArgument& argument: arguments) {
		if (argument.unbound) {
			throw DependentClassInstance("an instance of " + quoted + " that depends on a constant template parameter of a "
				"function template is not lowered yet");
		}
		dependent = dependent || (argument.isType && isDeductionMarker(argument.type));
	}
	if (dependent && arguments.size() < parameters.size()) {
		throw ClassTemplateError("an instance of " + quoted + " that depends on the parameters of a function "
			"template and leaves an argument to its default is not lowered yet");
	}

	// Saves what the binding of a parameter replaces, and puts it back.
	struct Bindings {
		Parser& parser;
		struct Saved {
			std::string key;
			std::optional<Type> type;
			std::optional<EnumConstant> constant;
		};
		std::vector<Saved> saved;
		Bindings(Parser& owner, const std::vector<TemplateParameter>& parameters): parser(owner) {
			for (const TemplateParameter& parameter: parameters) {
				Saved entry;
				entry.key = "<template " + parameter.name + ">";
				if (const auto type = parser._typedefs.find(entry.key); type != parser._typedefs.end()) {
					entry.type = type->second;
				}
				if (const auto constant = parser._enumConstants.find(entry.key); constant != parser._enumConstants.end()) {
					entry.constant = constant->second;
				}
				saved.push_back(std::move(entry));
			}
		}
		~Bindings() {
			for (const Saved& entry: saved) {
				entry.type ? void(parser._typedefs[entry.key] = *entry.type) : void(parser._typedefs.erase(entry.key));
				entry.constant ? void(parser._enumConstants[entry.key] = *entry.constant)
					: void(parser._enumConstants.erase(entry.key));
			}
		}
	};
	struct Guard {
		Parser& parser;
		Context saved;
		Guard(Parser& owner, Context&& next): parser(owner), saved(std::move(next)) { parser.swapContext(saved); }
		~Guard() { parser.swapContext(saved); }
	};

	struct Defined {
		Parser& parser;
		const std::set<std::string>* saved;
		Defined(Parser& owner, const std::set<std::string>* names): parser(owner), saved(owner._definedNames) {
			parser._definedNames = names;
		}
		~Defined() { parser._definedNames = saved; }
	};
	const auto fresh = [&](std::vector<Token> tokens) {
		Context next;
		next.tokens = std::move(tokens);
		reserveSplits(next.tokens);
		next.namespacePath = declaration.namespacePath;
		next.blocks.emplace_back();
		return next;
	};

	try {
		for (size_t index = arguments.size(); index < parameters.size(); ++index) {
			const TemplateParameter& parameter = parameters[index];
			if (!parameter.hasDefault) {
				throw CompileError("too few template arguments for " + quoted + ": \"" + parameter.name
					+ "\" has no default");
			}
			// Only the parameters before this one are in scope.
			const Bindings bindings(*this, parameters);
			const Defined defined(*this, &declaration.declaredBefore);
			const Guard guard(*this, fresh(parameter.defaultTokens));
			bindParameters(declaration.name, std::vector<TemplateParameter>(parameters.begin(), parameters.begin() + index),
				arguments);
			TemplateArgument argument;
			argument.isType = parameter.isType;
			if (parameter.isType) {
				argument.type = parseType();
				if (argument.type.isPointer || argument.type.arrayLength || argument.type.resource != ResourceKind::None
					|| !argument.type.enumName.empty() || argument.type.addressSpace != AddressSpace::None
					|| argument.type.isVolatile) {
					throw CompileError("a default that is a pointer, an array, a resource, an enum, volatile or in "
						"an address space is not lowered");
				}
			} else {
				const ExpressionPtr value = parseAssignment();
				argument.value = value->kind == ExpressionKind::BoolLiteral ? (value->boolValue ? 1 : 0)
					: evaluateConstant(*value, false);
			}
			if (!at(TokenKind::EndOfFile)) {
				throw CompileError("unexpected \"" + std::string(current().text) + "\" in a default argument");
			}
			if (argument.isType && isDeductionMarker(argument.type)) {
				throw CompileError("a default that depends on a function template parameter is not lowered yet");
			}
			arguments.push_back(std::move(argument));
		}
	} catch (const CompileError& error) {
		throw ClassTemplateError("in " + quoted + " (declared at line " + std::to_string(declaration.line) + "): "
			+ error.what());
	}

	// "::" separates a struct's namespace from its members everywhere, so an argument's own is not spelled.
	std::string key;
	for (size_t i = 0; i < arguments.size(); ++i) {
		const TemplateArgument& argument = arguments[i];
		std::string text = argument.isType ? typeName(argument.type)
			: std::to_string(argument.value);
		for (size_t at = text.find("::"); at != std::string::npos; at = text.find("::", at + 1)) {
			text.replace(at, 2, "$");
		}
		key += (i ? "," : "") + text;
	}
	const std::string spelled = declaration.spelled + "<" + key + ">";
	const std::string instance = declaration.name.substr(0, declaration.name.size() - declaration.spelled.size()) + spelled;
	if (dependent) {
		// While a function template's signature is read to deduce its parameters, an instance over one of them
		// is a marker type, like the parameter itself; deduction matches an argument's struct against what it
		// records. No struct is made.
		Type marker;
		marker.namedType = "<" + instance + ">";
		_typedefs[instance] = marker;
		_declared[instance] = "typedef";
		_dependentInstances[marker.namedType] = DependentInstance { declaration.name, arguments };
		return instance;
	}
	if (_unit.findStruct(instance) || _classInProgress.count(instance)) {
		return instance;
	}
	if (_frozenClasses) {
		if (_seedKeys.count(instance)) {
			throw ClassTemplateError("the instance \"" + instance + "\" of " + quoted + " is named inside a function "
				"template, but its argument types are declared after the template; declare them above it");
		}
		throw NeedClassInstance({ declaration.name, arguments, instance });
	}
	// clang reads a name from the definition, and a function again by argument-dependent lookup where the
	// instance is made. Neither is modelled for what was declared after the template, so a program that
	// could tell the two apart is refused rather than given another function's or constant's value.
	if (usingCount(declaration.namespacePath, declaration.mentioned, declaration.aliasTargets) != declaration.usingsBefore) {
		throw ClassTemplateError("a using declaration or directive is made after " + quoted + " in a scope it reads "
			"names from; write it above the template");
	}
	{
		// An unqualified call that is not a member call or a declarator (a member of this is one too) of a name the file declares
		// a function of above the template (a library function is not found this way). Whether anything of that
		// name comes later is known only at the end of the file.
		for (size_t i = 0; i + 1 < declaration.tokens.size(); ++i) {
			if (declaration.tokens[i].kind != TokenKind::Identifier || declaration.tokens[i + 1].kind != TokenKind::LParen
				|| (i > 0 && declaration.tokens[i - 1].kind == TokenKind::Identifier
					&& !std::set<std::string_view>{ "return", "else", "case", "do", "throw", "sizeof", "new" }.count(declaration.tokens[i - 1].text))
				|| (i > 0 && (declaration.tokens[i - 1].kind == TokenKind::Dot || declaration.tokens[i - 1].kind == TokenKind::Arrow
					|| declaration.tokens[i - 1].kind == TokenKind::ColonColon))) {
				continue;
			}
			const std::string call(declaration.tokens[i].text);
			bool declaredAbove = false;
			for (const std::string& key: declaration.declaredBefore) {
				const auto kind = _declared.find(key);
				declaredAbove = declaredAbove || (kind != _declared.end() && kind->second == "function"
					&& unqualifiedName(key) == call);
			}
			if (declaredAbove) {
				_adlChecks.push_back({ declaration.name, declaration.order, call });
			}
		}
	}
	// Apple stops at 1024.
	if (_classDepth >= 64) {
		throw ClassTemplateError("the instantiation of " + quoted + " nests deeper than 64 levels, at \"" + instance + "\"");
	}

	_spellings->push_back(spelled);
	const std::string_view view = _spellings->back();
	std::vector<Token> tokens = declaration.tokens;
	tokens[1].text = view;
	// The name inside the body means the instance (the injected class name), and is what a constructor is
	// declared by.
	for (size_t i = 2; i + 1 < tokens.size(); ++i) {
		const TokenKind before = tokens[i - 1].kind;
		if (tokens[i].kind == TokenKind::Identifier && tokens[i].text == declaration.spelled
			&& before != TokenKind::Dot && before != TokenKind::Arrow && before != TokenKind::ColonColon
			&& tokens[i + 1].kind != TokenKind::Less) {
			tokens[i].text = view;
		}
	}

	const size_t helpersBefore = _unit.helpers.size();
	const size_t templatesBefore = _unit.templates.size();
	const size_t orderBefore = _functionOrder;
	_classInProgress.insert(instance);
	++_classDepth;
	struct Done {
		Parser& parser;
		const std::string& instance;
		~Done() {
			--parser._classDepth;
			parser._classInProgress.erase(instance);
		}
	} done { *this, instance };
	try {
		const Bindings bindings(*this, parameters);
		const Defined defined(*this, &declaration.declaredBefore);
		const Guard guard(*this, fresh(std::move(tokens)));
		bindParameters(declaration.name, parameters, arguments);
		parseDeclaration();
		if (!at(TokenKind::EndOfFile)) {
			throw CompileError("unexpected \"" + std::string(current().text) + "\" after the template's struct");
		}
	} catch (const CompileError& error) {
		throw ClassTemplateError("in " + quoted + " (declared at line " + std::to_string(declaration.line)
			+ "), instantiated as \"" + spelled + "\": " + error.what());
	}
	_functionOrder = orderBefore;
	if (!_unit.findStruct(instance)) {
		throw ClassTemplateError("the instance \"" + instance + "\" of " + quoted + " did not declare a struct");
	}
	for (StructDecl& made: _unit.structs) {
		if (made.name == instance) {
			made.classTemplate = declaration.name;
			made.classArguments = arguments;
		}
	}
	for (size_t i = helpersBefore; i < _unit.helpers.size(); ++i) {
		// A nested instance's helpers were stamped with their own template's order when it was made.
		if (!_instanceHelpers.insert(i).second) {
			continue;
		}
		FunctionDecl& member = _unit.helpers[i];
		member.isLocal = true;
		member.order = declaration.order;
		member.globalsBefore = declaration.globalsBefore;
	}
	// A member function template of the instance is declared where the class template is, and reads the
	// class template's parameters as the instance bound them.
	for (size_t i = templatesBefore; i < _unit.templates.size(); ++i) {
		FunctionTemplate& member = _unit.templates[i];
		if (member.memberOf == instance) {
			member.order = member.bodyOrder = declaration.order;
			member.globalsBefore = declaration.globalsBefore;
			member.ownerTemplate = _classTemplates.at(declaration.name);
			member.ownerArguments = arguments;
		}
	}
	return instance;
}

// The name in the standard library's namespace that a resolved name denotes, or none when the
// file declares its own: an unqualified name is resolved as spelled, so a local or a declaration
// of that spelling hides the library's.
std::string Parser::libraryName(const std::string& resolved) const {
	if (resolved.rfind("metal::", 0) == 0) {
		return resolved.substr(7);
	}
	if (resolved.find("::") == std::string::npos && kindsOf(resolved).empty() && !isLocal(resolved)
		&& !memberHidesName(resolved)) {
		return resolved;
	}
	return std::string();
}

// "is_same<A, B>::value", "is_same_v<A, B>" and the other one- and two-type predicates, as a bool.
ExpressionPtr Parser::tryParseTraitValue() {
	if (kind() != TokenKind::Identifier && !at(TokenKind::ColonColon)) {
		return nullptr;
	}
	// Only a spelling that ends in "is_...<" is resolved: resolving every identifier here would throw
	// for a function name that is ambiguous until its arguments choose the overload.
	const auto tokenAt = [&](size_t offset) -> const Token& {
		return _tokens[std::min(_position + offset, _tokens.size() - 1)];
	};
	size_t end = at(TokenKind::ColonColon) ? 1 : 0;
	while (tokenAt(end + 1).kind == TokenKind::ColonColon && tokenAt(end + 2).kind == TokenKind::Identifier) {
		end += 2;
	}
	if (tokenAt(end).kind != TokenKind::Identifier || tokenAt(end).text.rfind("is_", 0) != 0
		|| tokenAt(end + 1).kind != TokenKind::Less) {
		return nullptr;
	}
	size_t nameTokens = 0;
	std::string name = peekResolved(nameTokens);
	name = libraryName(name);
	if (name.rfind("is_", 0) != 0 || lookahead(nameTokens).kind != TokenKind::Less) {
		return nullptr;
	}
	const bool variable = name.size() > 2 && name.compare(name.size() - 2, 2, "_v") == 0;
	const std::string trait = variable ? name.substr(0, name.size() - 2) : name;
	static const std::set<std::string> kTraits = { "is_same", "is_floating_point", "is_integral", "is_arithmetic",
		"is_signed", "is_unsigned", "is_scalar", "is_vector", "is_class", "is_enum", "is_const" };
	if (!kTraits.count(trait)) {
		throw CompileError("the trait \"" + trait + "\" is not lowered; mslc folds is_same, is_floating_point, "
			"is_integral, is_arithmetic, is_signed, is_unsigned, is_scalar, is_vector, is_class, is_enum and is_const");
	}
	const size_t startLine = line();
	for (size_t i = 0; i < nameTokens; ++i) {
		advance();
	}
	advance();
	std::vector<Type> operands;
	const size_t wanted = trait == "is_same" ? 2 : 1;
	for (;;) {
		operands.push_back(parseType());
		if (operands.size() < wanted) {
			expect(TokenKind::Comma, ("between the types of \"" + trait + "\"").c_str());
			continue;
		}
		break;
	}
	closeAngle(("to close the types of \"" + trait + "\"").c_str());
	if (!variable) {
		expect(TokenKind::ColonColon, ("after \"" + trait + "<...>\"").c_str());
		if (!atKeyword("value")) {
			throw CompileError("expected \"value\" after \"" + trait + "<...>::\"");
		}
		advance();
	}
	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::BoolLiteral;
	expression->line = startLine;
	try {
		expression->boolValue = traitHolds(trait, operands, _unit);
	} catch (const DependentTrait&) {
		if (!_inEnableIf) {
			throw;
		}
		_dependentCondition = true;
		expression->boolValue = true;
	}
	return expression;
}

// static_assert(condition) or static_assert(condition, "message"), the keyword consumed ([dcl.pre]): the condition is
// an integral constant expression, checked where it is read. A function template's body is read once per instance,
// with its arguments bound, so each instance checks its own; a false one is an error naming the message.
// In a statement (inStatement), two checks are left to the statement's lowering, and returned as the error that
// lowering it is: one in a member function of a class template's instance, whose body is instantiated only where
// the function is used ([temp.inst]) although mslc reads it with the class, and a condition the evaluator cannot
// fold, so a function never lowered is not refused for it.
std::string Parser::parseStaticAssert(bool inStatement) {
	const size_t assertLine = line();
	expect(TokenKind::LParen, "after \"static_assert\"");
	const ExpressionPtr condition = parseConditional();
	std::string message;
	if (match(TokenKind::Comma)) {
		if (!at(TokenKind::StringLiteral)) {
			throw CompileError("the message of a static_assert has to be a string literal");
		}
		// Adjacent literals are one string ([lex.string]).
		while (at(TokenKind::StringLiteral)) {
			std::string_view text = current().text;
			if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
				text = text.substr(1, text.size() - 2);
			}
			// The simple escapes are read as the characters they stand for ([lex.ccon]); any other is kept as spelled.
			for (size_t i = 0; i < text.size(); ++i) {
				if (text[i] != '\\' || i + 1 == text.size()) {
					message += text[i];
					continue;
				}
				const char escaped = text[++i];
				switch (escaped) {
					case 'n': message += '\n'; break;
					case 't': message += '\t'; break;
					case '"': case '\'': case '\\': case '?': message += escaped; break;
					default: message += '\\'; message += escaped; break;
				}
			}
			advance();
		}
	}
	std::string failure;
	try {
		if (evaluateConstant(*condition, false) == 0) {
			failure = "static assertion failed" + (message.empty() ? std::string() : ": " + message)
				+ " (line " + std::to_string(assertLine) + ")";
		}
	} catch (const CompileError& error) {
		// Only the evaluator's own refusal: a subclass carries something its catcher acts on.
		if (!inStatement || typeid(error) != typeid(CompileError)) {
			throw;
		}
		failure = error.what();
	}
	const bool deferred = inStatement && (!_classInProgress.empty() || failure.rfind("static assertion failed", 0) != 0);
	if (!failure.empty() && !deferred) {
		throw CompileError(failure);
	}
	expect(TokenKind::RParen, "to close \"static_assert(\"");
	expect(TokenKind::Semicolon, "after \"static_assert(...)\"");
	return failure;
}

// enable_if, enable_if_t, make_scalar and make_scalar_t, the name consumed and "<" next.
void Parser::parseTraitType(const std::string& name, Type& type) {
	const bool alias = name.size() > 2 && name.compare(name.size() - 2, 2, "_t") == 0;
	const bool enableIf = name.rfind("enable_if", 0) == 0;
	expect(TokenKind::Less, ("after \"" + name + "\"").c_str());
	Type result;
	if (enableIf) {
		bool holds = false;
		{
			const RestoreFlag noGreater(_noGreater, true);
			const RestoreFlag inside(_inEnableIf, true);
			const RestoreFlag dependent(_dependentCondition, false);
			const ExpressionPtr condition = parseConditional();
			// A condition that names a type parameter being deduced holds for now: the instance's own
			// signature is read again with the type, and that read decides.
			holds = _dependentCondition || evaluateConstant(*condition, false) != 0;
		}
		result.scalar = ScalarKind::Void;
		if (match(TokenKind::Comma)) {
			result = parseType();
		}
		closeAngle(("to close \"" + name + "<\"").c_str());
		if (!holds) {
			throw SubstitutionFailure("enable_if is not satisfied");
		}
	} else {
		const Type operand = parseType();
		closeAngle(("to close \"" + name + "<\"").c_str());
		if (isDeductionMarker(operand)) {
			if (!_inEnableIf) {
				throw DependentTrait("make_scalar depends on a type that is not deduced yet");
			}
			type = operand;
			if (!alias) {
				expect(TokenKind::ColonColon, ("after \"" + name + "<...>\"").c_str());
				if (!atKeyword("type")) {
					throw CompileError("expected \"type\" after \"" + name + "<...>::\"");
				}
				advance();
			}
			return;
		}
		if (operand.isPointer || operand.arrayLength || operand.resource != ResourceKind::None || operand.isAtomic
			|| operand.addressSpace != AddressSpace::None) {
			throw CompileError("make_scalar of a pointer, array, texture, sampler, atomic or address-space-qualified "
				"type is not lowered");
		}
		result = operand;
		result.isConst = false;
		if (result.matrixColumns == 0 && result.namedType.empty() && result.enumName.empty()) {
			result.vectorWidth = 0;
			result.isPacked = false;
		}
	}
	if (!alias) {
		expect(TokenKind::ColonColon, ("after \"" + name + "<...>\"").c_str());
		if (!atKeyword("type")) {
			throw CompileError("expected \"type\" after \"" + name + "<...>::\"");
		}
		advance();
	}
	const Type qualifiers = type;
	type = result;
	type.isConst = type.isConst || qualifiers.isConst;
	type.isConstexpr = qualifiers.isConstexpr;
	type.addressSpace = qualifiers.addressSpace;
}

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
	const StructDecl* const enclosing = _structBody;
	_structBody = &decl;
	struct Restore {
		const StructDecl*& slot;
		const StructDecl* value;
		~Restore() { slot = value; }
	} restore { _structBody, enclosing };
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

		// "float x, y;" : each declarator after the first starts from the type
		// before the first one's stars, and has its own attributes.
		Type base;
		const Type shared = parseType(false, false, &base);
		for (bool first = true;; first = false) {
			StructField field;
			field.type = shared;
			if (!first) {
				field.type = base;
				parseDeclaratorStars(field.type);
			}
			if (field.type.isAtomic && !field.type.isPointer) {
				throw CompileError("an atomic member of a struct (\"" + decl.name + "\") is not lowered yet; "
					"mslc lowers a device atomic_uint or atomic_int pointer parameter");
			}
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
			if (field.type.isPointer && (!first || at(TokenKind::Comma))) {
				throw CompileError("a pointer member (\"" + field.name + "\" in \"" + decl.name + "\") declared "
					"with several declarators is not lowered yet; declare each on its own line");
			}

			// The brackets before the name, as in "float[2] a;", are not C++, and
			// would otherwise be joined or dropped by the suffix read below.
			if (field.type.arrayLength && !field.type.isStdArray) {
				throw CompileError("the array member \"" + field.name + "\" of \"" + decl.name
					+ "\" puts its length before the name; write the length after it, as in float a[2]");
			}
			if (field.type.isStdArray && !field.type.isPointer && at(TokenKind::LBracket)
				&& lookahead().kind != TokenKind::LBracket) {
				throw CompileError("the member \"" + field.name + "\" of \"" + decl.name
					+ "\" is an array of array<T, N>, which is not lowered");
			}
			if (field.type.isStdArray && field.type.isPointer) {
				throw CompileError("the member \"" + field.name + "\" of \"" + decl.name
					+ "\" is a pointer to array<T, N>, which is not lowered");
			}
			if (field.type.isStdArray ? false : parseArrayDimensions(field.type)) {
				throw CompileError("the array member \"" + field.name + "\" of \"" + decl.name
					+ "\" needs a length: a member cannot be an array of unknown size");
			}

			// An attribute list and an array suffix both open with a bracket, but an
			// attribute list opens with two, which is what tells them apart.
			if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket) {
				field.attributes = parseFieldAttributes();
			}
			decl.fields.push_back(std::move(field));
			if (!match(TokenKind::Comma)) {
				break;
			}
		}

		match(TokenKind::Semicolon);
	}

	expect(TokenKind::RBrace, "to close a struct body");
}

// "template <...> R f(...) const {...}" in a struct body. Like a member function it is read once the struct
// is complete, but as a function template: its tokens are kept, and a call that selects it has it read with
// its arguments bound, the object being a first parameter called this (see parseInstance).
void Parser::parseMemberTemplate(StructDecl& decl) {
	const std::string quoted = "\"" + decl.name + "\"";
	if (unqualifiedName(decl.name).empty()) {
		throw CompileError("a member function template of an unnamed struct is not lowered");
	}
	advance();
	if (!at(TokenKind::Less)) {
		throw CompileError("an explicit instantiation of a member template is not lowered (in " + quoted + ")");
	}
	if (lookahead().kind == TokenKind::Greater) {
		throw CompileError("an explicit specialization of a member template is not lowered (in " + quoted + ")");
	}
	FunctionTemplate declaration;
	declaration.line = line();
	const size_t headerStart = _position;
	parseTemplateHeader(declaration);
	if (atKeyword("template") || atKeyword("using") || atKeyword("struct") || atKeyword("class")
		|| atKeyword("union") || atKeyword("typedef") || atKeyword("enum")) {
		throw CompileError("a member template that is not a function is not lowered (in " + quoted + ")");
	}
	size_t ahead = 0;
	for (;;) {
		if (lookahead(ahead).kind == TokenKind::LBracket && lookahead(ahead + 1).kind == TokenKind::LBracket) {
			int depth = 0;
			while (lookahead(ahead).kind != TokenKind::EndOfFile) {
				if (lookahead(ahead).kind == TokenKind::LBracket) {
					++depth;
				} else if (lookahead(ahead).kind == TokenKind::RBracket && --depth == 0) {
					++ahead;
					break;
				}
				++ahead;
			}
			continue;
		}
		if (lookahead(ahead).kind == TokenKind::Identifier && lookahead(ahead).text == "__attribute__"
			&& lookahead(ahead + 1).kind == TokenKind::LParen) {
			++ahead;
			int depth = 0;
			while (lookahead(ahead).kind != TokenKind::EndOfFile) {
				if (lookahead(ahead).kind == TokenKind::LParen) {
					++depth;
				} else if (lookahead(ahead).kind == TokenKind::RParen && --depth == 0) {
					++ahead;
					break;
				}
				++ahead;
			}
			continue;
		}
		if (lookahead(ahead).kind != TokenKind::Identifier) {
			break;
		}
		const std::string_view word = lookahead(ahead).text;
		if (word == "static") {
			declaration.staticMember = true;
			++ahead;
			continue;
		}
		if (word == "explicit" || word == "virtual" || word == "friend") {
			throw CompileError("a member template declared " + std::string(word) + " is not lowered (in " + quoted + ")");
		}
		if (word == "inline" || word == "constexpr") {
			++ahead;
			continue;
		}
		break;
	}
	if (lookahead(ahead).kind == TokenKind::Identifier && lookahead(ahead).text == unqualifiedName(decl.name)
		&& lookahead(ahead + 1).kind == TokenKind::LParen) {
		if (declaration.staticMember) {
			throw CompileError("a constructor template cannot be static (in " + quoted + ")");
		}
		declaration.constructorMember = true;
		decl.hasConstructors = true;
	}
	const std::string name = readTemplateFunction(declaration, headerStart, true);
	if (declaration.constructorMember && !declaration.hasBody) {
		throw CompileError("constructor template \"" + decl.name + "::" + name + "\" is declared and never defined");
	}
	declaration.name = decl.name + "::" + name;
	declaration.memberOf = decl.name;
	declaration.order = declaration.bodyOrder = _functionOrder++;
	declaration.globalsBefore = _unit.globals.size();
	if (declaration.constructorMember) {
		ConstructorInfo& constructors = _constructors[decl.name];
		constructors.helper = declaration.name;
		constructors.hasTemplate = true;
		_unit.functionTemplates.insert(declaration.name);
		_pendingMemberTemplates.push_back(std::move(declaration));
		return;
	}
	if (const auto earlier = _memberFunctions.find(name);
		earlier != _memberFunctions.end() && earlier->second != declaration.staticMember) {
		throw CompileError("\"" + declaration.name + "\" is declared both static and not static, which Apple takes and mslc does not lower");
	}
	_memberFunctions[name] = declaration.staticMember;
	if (declaration.staticMember) {
		_unit.functionTemplates.insert(declaration.name);
	}
	_memberTemplateNames.insert(name);
	_pendingMemberTemplates.push_back(std::move(declaration));
}

// A member that is not a field: a constructor, a member function or a member
// that cannot be lowered. False, with nothing consumed, when the member is a
// field. A function is skipped here and parsed once the struct is complete, since
// its body may name fields declared after it and the struct's own type.
bool Parser::parseStructFunction(StructDecl& decl) {
	const std::string quoted = "\"" + decl.name + "\"";
	if (atKeyword("template")) {
		parseMemberTemplate(decl);
		return true;
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
			if (at(TokenKind::Ampersand) && lookahead().kind == TokenKind::Identifier && lookahead().text == "operator") {
				throw CompileError("an operator overload returning a reference is not lowered (in " + quoted + ")");
			}
			if (atKeyword("operator")) {
				if (isStatic || lookahead(1).kind != TokenKind::LBracket
					|| lookahead(2).kind != TokenKind::RBracket || lookahead(3).kind != TokenKind::LParen) {
					throw CompileError("an operator overload is not lowered (in " + quoted + ")");
				}
				_position += 3; functionName = "operator[]"; isFunction = true;
			} else if (kind() == TokenKind::Identifier) {
				functionName = std::string(advance().text);
				isFunction = at(TokenKind::LParen);
			}
		} catch (const ClassTemplateError&) {
			throw;
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
	std::vector<FunctionTemplate> templates = std::move(_pendingMemberTemplates);
	_pendingMemberTemplates.clear();
	if (starts.empty() && templates.empty()) {
		return;
	}

	_memberStruct = decl.name;
	std::set<std::string> fields;
	std::vector<std::string> fieldOrder;
	for (const StructField& field: decl.fields) {
		fields.insert(field.name);
		fieldOrder.push_back(field.name);
	}

	const size_t resume = _position;
	const size_t helpersBefore = _unit.helpers.size();
	_memberFields = &fields;
	for (const size_t start: starts) {
		parseStructMember(decl, start);
	}
	_memberFields = nullptr;
	const size_t structOrder = _functionOrder++;
	for (size_t i = helpersBefore; i < _unit.helpers.size(); ++i) {
		if (!_instanceHelpers.count(i)) {
			_unit.helpers[i].order = structOrder;
		}
	}
	// A member template is read where the struct ends, with every member it can name in hand.
	if (!templates.empty()) {
		const std::shared_ptr<const void> names = captureNames();
		for (FunctionTemplate& member: templates) {
			member.order = member.bodyOrder = structOrder;
			member.names = names;
			member.memberFieldOrder = fieldOrder;
			member.memberFields = fields;
			member.memberFunctions = _memberFunctions;
			member.spellings = _spellings;
			_unit.templates.push_back(std::move(member));
		}
	}
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
		if (atKeyword("operator")) {
			advance();
			expect(TokenKind::LBracket, "after 'operator'");
			expect(TokenKind::RBracket, "after '['");
			memberName = "operator[]";
		} else {
			memberName = std::string(advance().text);
		}
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
	if (memberName == "operator[]" && fn.parameters.size() != 2) {
		throw CompileError(quoted + " must take exactly one parameter");
	}

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
			ExpressionPtr value = parseAssignment();
			expect(TokenKind::RParen, "to close a member initialiser");
			initialisers.emplace_back(field, std::move(value));
		} while (match(TokenKind::Comma));
	}

	if (!isConstructor && !isStatic) {
		bool sawConst = false;
		AddressSpace space = AddressSpace::None;
		for (; kind() == TokenKind::Identifier; advance()) {
			const std::string_view word = current().text;
			if (word == "const") {
				if (sawConst) { throw CompileError("duplicate 'const' qualifier on " + quoted); }
				sawConst = true;
			} else if (word == "volatile") {
				throw CompileError("a volatile member function is not lowered (" + quoted + ")");
			} else if (const auto found = addressSpaceFor(word)) {
				if (space != AddressSpace::None) { throw CompileError("duplicate address-space qualifier on " + quoted); }
				if (*found != AddressSpace::Thread && *found != AddressSpace::Constant) {
					throw CompileError(quoted + " is qualified " + std::string(word) + "; only thread and constant member functions are lowered");
				}
				space = *found;
			} else { break; }
		}
		if (!sawConst) {
			throw CompileError(quoted + " is not const; mslc takes the object by value, so a member function "
				"that could write it is not lowered (mark it const)");
		}
		fn.parameters[0].type.isConst = true;
		fn.parameters[0].type.addressSpace = space == AddressSpace::Constant ? AddressSpace::Constant : AddressSpace::Thread;
		fn.parameters[0].isReference = space == AddressSpace::Constant;
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
		fn.isLocal = true;
		_unit.helpers.push_back(std::move(fn));
		return;
	}

	fn.body = parseCompoundStatement(true);
	fn.isLocal = true;
	if (!isConstructor) {
		_unit.helpers.push_back(std::move(fn));
		return;
	}

	if (fn.parameters.empty() && initialisers.empty() && fn.body->children.empty()) {
		constructors->hasDefault = true;
		return;
	}
	if (constructors->hasNonTemplate) {
		throw CompileError("overloading the constructors of \"" + decl.name + "\" is not lowered yet");
	}
	constructors->hasNonTemplate = true;
	constructors->helper = fn.name;
	constructors->parameters = fn.parameters.size();

	const auto rewriteReturn = [&](const auto& self, Statement& s) -> void {
		if (s.kind == StatementKind::Return) {
			if (s.expression) {
				throw CompileError(quoted + " is a constructor and cannot return a value");
			}
			s.expression = makeIdentifier("this", s.line);
		}
		for (StatementPtr& child: s.children) { self(self, *child); }
		if (s.thenBranch) { self(self, *s.thenBranch); }
		if (s.elseBranch) { self(self, *s.elseBranch); }
		if (s.forBody) { self(self, *s.forBody); }
		if (s.whileBody) { self(self, *s.whileBody); }
		for (SwitchCase& sc: s.switchCases) {
			for (StatementPtr& child: sc.body) { self(self, *child); }
		}
		for (StatementPtr& child: s.switchPreamble) { self(self, *child); }
	};
	rewriteReturn(rewriteReturn, *fn.body);

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
	const bool constructs = arguments.empty()
		? (info.parameters == 0 || info.hasTemplate) && !info.helper.empty() && !info.hasDefault
		: !info.helper.empty();
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
			&& a.namedType == b.namedType && a.isConst == b.isConst
			&& a.enumName == b.enumName && a.isAtomic == b.isAtomic
			&& a.arrayLength == b.arrayLength && a.innerArrayLengths == b.innerArrayLengths
			&& a.isStdArray == b.isStdArray;
	}

	// Names the typedef and enum support has to keep distinct from everything else.
	constexpr char kAnonymousStructTypedef[] = "typedef of an anonymous struct";

	bool isTypeSupportKind(std::string_view what) {
		return what == "typedef" || what == "enum" || what == "enum constant"
			|| what == kAnonymousStructTypedef;
	}

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
	if (const auto enumType = _enumTypes.find(std::string(text)); enumType != _enumTypes.end()) {
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
			const std::string key = enclosing == std::string::npos ? last : tag.substr(0, enclosing + 2) + last;
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
	// Until the closing brace an enumerator has the type of its initializer, or the previous enumerator's
	// when it has none ([dcl.enum]/5), so one given an unsigned value is unsigned in the initializers after it.
	ScalarKind nextKind = ScalarKind::Int;
	int64_t lowest = 0;
	int64_t highest = 0;
	std::vector<std::string> names;
	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected an enumerator name, found " + std::string(tokenKindName(kind())));
		}

		const std::string name = qualify(std::string(advance().text));
		int64_t value = next;
		ScalarKind kind = nextKind;
		if (match(TokenKind::Assign)) {
			const ExpressionPtr initializer = parseAssignment();
			value = evaluateConstant(*initializer, false);
			kind = containsUnsigned(*initializer) ? ScalarKind::UInt : ScalarKind::Int;
		}
		if (value > UINT_MAX || value < -INT_MAX) {
			throw CompileError("the enumerator \"" + name + "\" does not fit in an int or an unsigned int");
		}

		declareName(name, "enum constant");
		_enumConstants[name] = {value, kind, tag.empty() ? std::string("<anonymous>") : tag};
		names.push_back(name);
		lowest = std::min(lowest, value);
		highest = std::max(highest, value);
		next = value + 1;
		nextKind = kind;

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
	}
	// After it, every enumerator has the enum's type, which promotes to its underlying one.
	for (const std::string& name : names) {
		_enumConstants[name].kind = underlying;
	}
	_lastEnumUnderlying = underlying;
	if (!tag.empty()) {
		_enumUnderlying[tag] = underlying;
	}
	return tag;
}

// Whether an unsigned operand (a sizeof, a u literal, or a named unsigned constant such as an unsigned template
// parameter) is anywhere in a constant expression, which makes the operation around it unsigned ([expr.arith.conv]).
// The evaluator works in signed arithmetic and refuses the operations whose unsigned result would differ.
bool Parser::containsUnsigned(const Expression& expression) const {
	if (expression.kind == ExpressionKind::IntLiteral) {
		return expression.isSizeof || expression.intKind == ScalarKind::UInt || expression.intKind == ScalarKind::ULong;
	}
	if (expression.kind == ExpressionKind::Identifier) {
		const ConstantInt* found = findConstantInt(expression);
		return found && (found->kind == ScalarKind::UInt || found->kind == ScalarKind::ULong);
	}
	if (expression.kind == ExpressionKind::Construct) {
		// A cast to a type no wider than an int gives an int, which wraps nothing.
		const ScalarKind target = expression.constructType ? expression.constructType->scalar : ScalarKind::Int;
		return (target == ScalarKind::UInt || target == ScalarKind::ULong)
			&& expression.arguments.size() == 1 && containsUnsigned(*expression.arguments.front());
	}
	// A conditional's type is that of its two values after the usual arithmetic conversions ([expr.cond]): either
	// one being unsigned makes it unsigned, whichever is chosen. Its condition does not take part.
	if (expression.kind == ExpressionKind::Conditional) {
		return expression.arguments.size() == 2
			&& (containsUnsigned(*expression.arguments[0]) || containsUnsigned(*expression.arguments[1]));
	}
	return (expression.left && containsUnsigned(*expression.left))
		|| (expression.right && containsUnsigned(*expression.right));
}

// The integer constant a name denotes, looked up as namedConstantValue looks it up, or nothing.
const Parser::ConstantInt* Parser::findConstantInt(const Expression& identifier) const {
	if (!identifier.fileScope) {
		for (auto scope = _blocks.rbegin(); scope != _blocks.rend(); ++scope) {
			if (scope->names.count(identifier.name)) {
				const auto constant = scope->constants.find(identifier.name);
				return constant == scope->constants.end() ? nullptr : &constant->second;
			}
		}
	}
	const auto constant = _constantInts.find(identifier.name);
	return constant == _constantInts.end() ? nullptr : &constant->second;
}

// Whether a sizeof is anywhere under an expression, through casts of any type.
static bool mentionsSizeof(const Expression& expression) {
	if (expression.kind == ExpressionKind::IntLiteral) {
		return expression.isSizeof;
	}
	for (const ExpressionPtr& argument: expression.arguments) {
		if (mentionsSizeof(*argument)) {
			return true;
		}
	}
	return (expression.left && mentionsSizeof(*expression.left))
		|| (expression.right && mentionsSizeof(*expression.right));
}

// The range an integer type holds, for a cast of a constant to it. Nothing for a type that is not
// an integer one.
static bool integerRange(ScalarKind kind, int64_t& low, int64_t& high) {
	low = 0;
	switch (kind) {
		case ScalarKind::Char: low = INT8_MIN; high = INT8_MAX; return true;
		case ScalarKind::UChar: high = UINT8_MAX; return true;
		case ScalarKind::Short: low = INT16_MIN; high = INT16_MAX; return true;
		case ScalarKind::UShort: high = UINT16_MAX; return true;
		case ScalarKind::Int: low = INT_MIN; high = INT_MAX; return true;
		case ScalarKind::UInt: high = UINT32_MAX; return true;
		case ScalarKind::Long: low = INT64_MIN; high = INT64_MAX; return true;
		case ScalarKind::ULong: high = INT64_MAX; return true;
		default: return false;
	}
}

// Folds the integer constant expressions an enumerator, an attribute index or an
// array length may be. Every step is held to what an int holds, so a result that
// would wrap is an error here, and that is narrower than Apple's compiler, which
// widens. A `u` literal is only accepted as the whole expression, since mixing it
// with a signed operand changes the arithmetic to unsigned, which this does not
// model.
int64_t Parser::evaluateConstant(const Expression& expression, bool nested, bool attribute) const {
	const auto fit = [](int64_t value) {
		if (value > INT_MAX || value < -INT_MAX) {
			throw CompileError("an integer constant expression overflows an int");
		}
		return value;
	};

	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
			// A sizeof is a ulong, which unsigned arithmetic would wrap but this does not, so
			// the operators below refuse the one case where a signed result differs.
			if (expression.isSizeof) {
				return fit(static_cast<int64_t>(expression.intValue));
			}
			if (nested && expression.intValue > INT_MAX) {
				throw CompileError("an integer above INT_MAX inside a constant expression is not supported");
			}
			if (expression.intValue > INT64_MAX) {
				throw CompileError("an integer constant is too large");
			}
			return static_cast<int64_t>(expression.intValue);
		case ExpressionKind::BoolLiteral:
			// Apple takes a bool literal where an array length or an enumerator is, and refuses one inside an
			// attribute index (a named bool constant is accepted there).
			if (attribute) {
				throw CompileError("a bool is not an index; use an int or a uint");
			}
			return expression.boolValue ? 1 : 0;
		case ExpressionKind::Identifier:
			return namedConstantValue(expression, nested, attribute);
		case ExpressionKind::Index:
			return constantArrayElement(expression, nested, attribute);
		case ExpressionKind::Unary: {
			const int64_t operand = evaluateConstant(*expression.left, true, attribute);
			if ((expression.unaryOperator == UnaryOperator::Negate || expression.unaryOperator == UnaryOperator::BitNot)
				&& containsUnsigned(*expression.left)) {
				throw CompileError("an unsigned operand (a sizeof, a u literal or an unsigned constant), negated or "
					"complemented, wraps, and a constant expression that does so is not supported");
			}
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
			const int64_t left = evaluateConstant(*expression.left, true, attribute);

			// The right operand is evaluated only when the left does not decide
			// the result, so `0 && 1 / 0` is a constant and not a division by zero.
			if (expression.binaryOperator == BinaryOperator::LogicalAnd && left == 0) {
				return 0;
			}
			if (expression.binaryOperator == BinaryOperator::LogicalOr && left != 0) {
				return 1;
			}

			const int64_t right = evaluateConstant(*expression.right, true, attribute);
			// An unsigned operand makes the operation unsigned, so a negative operand converts to a huge one
			// and a negative result wraps; neither is modelled, and neither is guessed at. Otherwise the
			// signed result of every operator here is the unsigned one.
			if ((containsUnsigned(*expression.left) || containsUnsigned(*expression.right))
				&& (left < 0 || right < 0 || (expression.binaryOperator == BinaryOperator::Subtract && left < right))) {
				throw CompileError("a constant expression that mixes an unsigned operand with a negative value or "
					"subtracts below zero would wrap as an unsigned one, which is not supported");
			}
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
		case ExpressionKind::Conditional: {
			// Only the value the condition chooses is evaluated ([expr.cond]), so the other may be one no constant
			// expression holds, such as a division by zero.
			if (expression.arguments.size() != 2) {
				break;
			}
			// The condition is a bool, not the index, so a bool literal is fine there in an attribute index.
			const int64_t condition = evaluateConstant(*expression.left, true, false);
			const int64_t value = evaluateConstant(*expression.arguments[condition != 0 ? 0 : 1], nested, attribute);
			// An unsigned value on either side makes the conditional unsigned, so a negative one chosen converts
			// to a huge one, which is not modelled.
			if (value < 0 && containsUnsigned(expression)) {
				throw CompileError("a constant expression that mixes an unsigned operand with a negative value or "
					"subtracts below zero would wrap as an unsigned one, which is not supported");
			}
			return value;
		}
		case ExpressionKind::Sizeof:
			throw CompileError("the size of an expression is worked out after parsing, so it is not an "
				"array length or an attribute index; write sizeof(<type>)");
		case ExpressionKind::Construct: {
			// A cast is only taken of a sizeof, which is the one ulong an attribute index or an array
			// length is written from; (uint)sizeof(S) and int(sizeof(S)) hold the same value.
			int64_t low = 0;
			int64_t high = 0;
			if (expression.arguments.size() != 1 || !mentionsSizeof(*expression.arguments.front())
				|| !expression.constructType || expression.constructType->isPointer
				|| expression.constructType->arrayLength || !expression.constructType->isScalar()
				|| !expression.constructType->namedType.empty() || !expression.constructType->enumName.empty()
				|| !integerRange(expression.constructType->scalar, low, high)) {
				break;
			}
			const int64_t operand = evaluateConstant(*expression.arguments.front(), nested, attribute);
			if (operand < low || operand > high) {
				throw CompileError("a cast of a constant to a type that cannot hold it wraps, which is "
					"not supported");
			}
			return operand;
		}
		default:
			break;
	}

	throw CompileError("not an integer constant expression");
}

// A name in a constant expression: an enumerator the parser already replaced with its value, or an
// integer declared const or constexpr with an initialiser that folded. The innermost block that
// declares the name decides, so a local that hides a constant is not one. Only an int-sized type
// takes part in arithmetic: an unsigned or wider one would need unsigned arithmetic, which
// evaluateConstant does not model, so it is accepted as the whole expression and nothing else.
int64_t Parser::namedConstantValue(const Expression& identifier, bool nested, bool attribute) const {
	const std::string& name = identifier.name;
	const ConstantInt* found = nullptr;
	std::string global = name;
	// Inside a struct body a member declared above hides a file-scope constant of its name, and a
	// non-static member is not a constant (Apple: invalid use of non-static data member).
	if (_structBody && !identifier.fileScope) {
		for (const StructField& field: _structBody->fields) {
			if (field.name == name) {
				throw CompileError("\"" + name + "\" is a non-static data member, not a constant");
			}
		}
	}
	if (!identifier.fileScope) {
		for (auto scope = _blocks.rbegin(); scope != _blocks.rend(); ++scope) {
			const auto entry = scope->names.find(name);
			if (entry == scope->names.end()) {
				continue;
			}
			const auto constant = scope->constants.find(name);
			if (constant == scope->constants.end()) {
				throw CompileError("\"" + name + "\" is a variable, not a constant: only a const or constexpr "
					"integer with a constant initialiser is");
			}
			found = &constant->second;
			break;
		}
	}
	if (!found) {
		const auto constant = _constantInts.find(global);
		if (constant == _constantInts.end()) {
			throw CompileError("\"" + name + "\" is not an integer constant this source declares before this "
				"one (a const or constexpr integer with a constant initialiser is)");
		}
		found = &constant->second;
	}

	if (attribute && (found->kind == ScalarKind::Long || found->kind == ScalarKind::ULong)) {
		throw CompileError("a long is not an index; use an int or a uint");
	}
	if (nested && (found->value > INT_MAX || found->value < -INT_MAX)) {
		throw CompileError("an integer constant outside what an int holds inside a constant expression is not "
			"supported");
	}
	return found->value;
}

// An element of a constant integer array, `table[2]`: the array is a const or constexpr one-dimensional
// array of integers with a brace initialiser that folded (recordConstantArray), and the index is itself
// a constant expression inside the array's bounds. The name is looked up the way namedConstantValue
// looks up an integer constant, so a local or a member that hides the array is not one.
int64_t Parser::constantArrayElement(const Expression& element, bool nested, bool attribute) const {
	if (element.isDereference || !element.left || element.left->kind != ExpressionKind::Identifier
		|| element.arguments.size() != 1) {
		throw CompileError("not an integer constant expression");
	}
	const Expression& identifier = *element.left;
	const std::string& name = identifier.name;
	const ConstantIntArray* found = nullptr;
	if (_structBody && !identifier.fileScope) {
		for (const StructField& field: _structBody->fields) {
			if (field.name == name) {
				throw CompileError("\"" + name + "\" is a non-static data member, not a constant");
			}
		}
	}
	if (!identifier.fileScope) {
		for (auto scope = _blocks.rbegin(); scope != _blocks.rend(); ++scope) {
			if (!scope->names.count(name)) {
				continue;
			}
			const auto array = scope->constantArrays.find(name);
			if (array == scope->constantArrays.end()) {
				throw CompileError("\"" + name + "\" is not a constant integer array: only a const or constexpr "
					"array of integers with a constant brace initialiser is");
			}
			found = &array->second;
			break;
		}
	}
	if (!found) {
		const auto array = _constantIntArrays.find(name);
		if (array == _constantIntArrays.end()) {
			throw CompileError("\"" + name + "\" is not a constant integer array this source declares before "
				"this one (a const or constexpr array of integers with a constant brace initialiser is)");
		}
		found = &array->second;
	}

	const int64_t index = evaluateConstant(*element.arguments.front(), true, false);
	if (index < 0 || static_cast<uint64_t>(index) >= found->values.size()) {
		throw CompileError("the index " + std::to_string(index) + " is outside the array \"" + name
			+ "\" of " + std::to_string(found->values.size()) + " elements");
	}
	if (attribute && (found->kind == ScalarKind::Long || found->kind == ScalarKind::ULong)) {
		throw CompileError("a long is not an index; use an int or a uint");
	}
	// An element's unsignedness is not tracked (containsUnsigned does not look into arrays), so an unsigned or
	// wider array is still taken only as the whole expression.
	const bool intSized = found->kind != ScalarKind::UInt && found->kind != ScalarKind::Long
		&& found->kind != ScalarKind::ULong;
	if (nested && !intSized) {
		throw CompileError("an element of an unsigned or wider array inside a constant expression is not supported");
	}
	return found->values[static_cast<size_t>(index)];
}

// A const or constexpr array of integers, one dimension, initialised by a brace list whose elements all
// fold: the elements are kept so a constant expression can index the array. Elements the list leaves out
// are zero, as C++ says. Anything else (a designator, a nested list, a value its type cannot hold, a list
// longer than the array) leaves the array unrecorded, so indexing it in a constant expression is refused.
void Parser::recordConstantArray(const VariableDeclaration& declaration, bool local) {
	const Type& type = declaration.type;
	if (!(type.isConst || type.isConstexpr || !local) || type.isPointer || type.arrayRank() != 1
		|| type.vectorWidth != 0 || !declaration.initializer
		|| declaration.initializer->kind != ExpressionKind::InitList) {
		return;
	}
	int64_t low = 0;
	int64_t high = 0;
	if (!integerRange(type.scalar, low, high)) {
		return;
	}
	const std::vector<InitializerElement>& elements = declaration.initializer->elements;
	if (elements.size() > *type.arrayLength) {
		return;
	}
	ConstantIntArray array;
	array.kind = type.scalar;
	array.values.assign(*type.arrayLength, 0);
	for (size_t i = 0; i < elements.size(); ++i) {
		if (!elements[i].fieldName.empty() || !elements[i].value) {
			return;
		}
		int64_t value = 0;
		try {
			value = evaluateConstant(*elements[i].value, false);
		} catch (const CompileError&) {
			return;
		}
		if (value < low || value > high) {
			return;
		}
		array.values[i] = value;
	}
	if (local) {
		_blocks.back().constantArrays[declaration.name] = std::move(array);
	} else {
		_constantIntArrays[declaration.name] = std::move(array);
	}
}

// An integer declared const or constexpr (or at file scope) with a constant initialiser may be
// named by a later array length; a value its own type cannot hold is left out.
void Parser::recordConstant(const VariableDeclaration& declaration, bool local) {
	const Type& type = declaration.type;
	if (type.arrayLength) {
		recordConstantArray(declaration, local);
		return;
	}
	if (!(type.isConst || type.isConstexpr || !local) || type.isPointer || (!local && type.isAtomic)
		|| !type.isScalar() || !type.namedType.empty() || !declaration.initializer) {
		return;
	}
	int64_t low = 0;
	int64_t high = 0;
	switch (type.scalar) {
		case ScalarKind::Bool: low = 0; high = 1; break;
		case ScalarKind::Char: low = INT8_MIN; high = INT8_MAX; break;
		case ScalarKind::UChar: high = UINT8_MAX; break;
		case ScalarKind::Short: low = INT16_MIN; high = INT16_MAX; break;
		case ScalarKind::UShort: high = UINT16_MAX; break;
		case ScalarKind::Int: low = INT32_MIN; high = INT32_MAX; break;
		case ScalarKind::UInt: high = UINT32_MAX; break;
		case ScalarKind::Long: low = INT64_MIN; high = INT64_MAX; break;
		case ScalarKind::ULong: high = INT64_MAX; break;
		default: return;
	}

	const Expression* initializer = declaration.initializer.get();
	while (initializer->kind == ExpressionKind::InitList && !initializer->constructType
		&& initializer->elements.size() == 1 && initializer->elements.front().fieldName.empty()) {
		initializer = initializer->elements.front().value.get();
	}
	if (initializer->kind == ExpressionKind::Construct && initializer->arguments.size() == 1
		&& initializer->constructType && initializer->constructType->scalar == type.scalar
		&& initializer->constructType->isScalar() && !initializer->constructType->isPointer) {
		initializer = initializer->arguments.front().get();
	}
	int64_t value = 0;
	if (!(initializer->kind == ExpressionKind::InitList && !initializer->constructType
		&& initializer->elements.empty())) {
		try {
			value = evaluateConstant(*initializer, false);
		} catch (const CompileError&) {
			return;
		}
	}
	if (type.scalar == ScalarKind::Bool && value != 0) {
		value = 1;
	}
	if (value < low || value > high) {
		return;
	}

	if (local) {
		_blocks.back().constants[declaration.name] = {value, type.scalar};
	} else {
		_constantInts[declaration.name] = {value, type.scalar};
	}
}

// A local or a parameter keeps its type for sizeof once the declarator is complete. An auto, a
// resource or an array with no length yet is left out, so a sizeof of it is not folded here.
void Parser::recordObjectType(const std::string& name, const Type& type) {
	if (name.empty() || type.isAuto || type.resource != ResourceKind::None || _blocks.empty()) {
		return;
	}
	_blocks.back().objectTypes[name] = type;
}

// The declared type of a name or of an element of one, as the innermost declaration that the name
// finds says. Nothing else is typed here (a member, a call, a vector component), and a name whose
// innermost declaration has no recorded type is not looked up further out, which would size a
// variable that hides it wrongly.
std::optional<Type> Parser::declaredObjectType(const Expression& operand) const {
	if (operand.kind == ExpressionKind::Index) {
		std::optional<Type> base = declaredObjectType(*operand.left);
		if (!base || (base->isPointer && base->arrayLength)) {
			return std::nullopt;
		}
		if (base->arrayLength) {
			base->dropOuterDimension();
		} else if (base->isPointer) {
			// A decayed "float m[2][3]" points at arrays of 3, whose lengths stay in the type.
			base->isPointer = false;
			base->dropOuterDimension();
		} else {
			return std::nullopt;
		}
		return base;
	}
	if (operand.kind != ExpressionKind::Identifier || operand.templateDependent) {
		return std::nullopt;
	}
	if (_structBody) {
		for (const StructField& field: _structBody->fields) {
			if (field.name == operand.name) {
				return std::nullopt;
			}
		}
	}
	if (!operand.fileScope) {
		for (auto scope = _blocks.rbegin(); scope != _blocks.rend(); ++scope) {
			if (!scope->names.count(operand.name)) {
				continue;
			}
			const auto type = scope->objectTypes.find(operand.name);
			return type == scope->objectTypes.end() ? std::nullopt : std::optional<Type>(type->second);
		}
	}
	for (const VariableDeclaration& global: _unit.globals) {
		if (global.name == operand.name) {
			const Type& type = global.type;
			if (type.isAuto || type.resource != ResourceKind::None) {
				return std::nullopt;
			}
			return type;
		}
	}
	return std::nullopt;
}

// Whether a constant expression has a long or ulong in it, which makes its
// type one. A shift has the type of its left operand alone.
static bool isSixtyFourBit(const Expression& expression) {
	switch (expression.kind) {
		case ExpressionKind::IntLiteral:
			return expression.intKind == ScalarKind::Long || expression.intKind == ScalarKind::ULong;
		case ExpressionKind::Unary:
			return isSixtyFourBit(*expression.left);
		case ExpressionKind::Construct:
			if (expression.constructType && expression.constructType->isScalar() && !expression.constructType->isPointer) {
				return expression.constructType->scalar == ScalarKind::Long
					|| expression.constructType->scalar == ScalarKind::ULong;
			}
			return false;
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
		value = evaluateConstant(*expression, false, isAttribute);
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

	// Apple's texture_buffer is access::read unless it says otherwise, and has no sampled form.
	const bool isBuffer = color != textures.end() && color->second == ResourceKind::TextureBuffer;
	std::string access = isBuffer ? "read" : "sample";
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
	if (isMultisampled && (access != "sample" || accessSpelled) && access != "read") {
		throw CompileError(name + " access::" + access + " is not lowered yet; a multisampled texture is read-only");
	}
	if (type.isDepth && (element->second != ScalarKind::Float || access != "sample")) {
		throw CompileError(name + "<" + component + (access == "sample" ? "" : ", access::" + access)
			+ "> is not lowered yet; mslc lowers depth textures of float with access::sample"
			+ (access == "sample" ? " (Apple has no depth half)" : ""));
	}
	// A multisampled texture is a sampled image whichever way read is spelled.
	type.textureAccess = isMultisampled ? TextureAccess::Sample : access == "write" ? TextureAccess::Write
		: access == "read" ? TextureAccess::Read
		: access == "read_write" ? TextureAccess::ReadWrite : TextureAccess::Sample;
	type.scalar = element->second;
	type.namedType = deducedFrom;
}

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

// The initialiser of an auto local, which the type is deduced from: "= value",
// or a single value in parentheses or braces. "= {v}" is a list, which Apple
// deduces as a different type, so it is left as one for the lowering to refuse.
void Parser::parseAutoInitializer(VariableDeclaration& declaration) {
	const std::string quoted = "\"" + declaration.name + "\"";
	if (match(TokenKind::Assign)) {
		const size_t from = _position;
		declaration.initializer = parseInitializer();
		noteAutoConstruction(declaration.name, from);
	} else if (match(TokenKind::LParen)) {
		std::vector<ExpressionPtr> arguments = parseArgumentList("to close the initialiser of an auto variable");
		if (arguments.size() != 1) {
			throw CompileError("the auto variable " + quoted + " needs exactly one value to deduce its type from");
		}
		declaration.initializer = std::move(arguments.front());
	} else if (at(TokenKind::LBrace)) {
		ExpressionPtr list = parseInitializerList();
		if (list->elements.size() != 1 || !list->elements.front().fieldName.empty()) {
			throw CompileError("the auto variable " + quoted + " needs exactly one value to deduce its type from");
		}
		declaration.initializer = std::move(list->elements.front().value);
	} else {
		throw CompileError("the variable " + quoted + " is declared with auto and needs an initialiser to deduce its type from");
	}
}

// True when the parenthesised list ahead reads as the parameters of a function
// declaration: every comma group is a type, then "(", an optional name and ")"
// ("float(a)"), as C++ resolves "float3 v(float(a));". Empty parentheses are
// not matched here. The position is restored.
bool Parser::parensDeclareFunction() {
	const size_t start = _position;
	advance();
	bool declares = true;
	do {
		if (atKeyword("const")) {
			advance();
		}
		size_t typeTokens = 0;
		Type probe;
		const bool named = kind() == TokenKind::Identifier || at(TokenKind::ColonColon);
		const std::string name = named ? peekResolved(typeTokens) : std::string();
		const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
			&& isLocal(name);
		if (!named || objectHidesTag(name)
			|| !(resolveTypeName(name, probe) || (_unit.findStruct(name) != nullptr && !localHidesStruct))
			|| lookahead(typeTokens).kind != TokenKind::LParen) {
			declares = false;
			break;
		}
		for (size_t i = 0; i <= typeTokens; ++i) {
			advance();
		}
		while (at(TokenKind::Star) || at(TokenKind::Ampersand)) {
			advance();
		}
		size_t nested = 0;
		while (at(TokenKind::LParen)) {
			++nested;
			advance();
		}
		if (kind() == TokenKind::Identifier) {
			advance();
		}
		while (at(TokenKind::LBracket) || (nested > 0 && at(TokenKind::RParen))) {
			if (at(TokenKind::RParen)) {
				--nested;
				advance();
				continue;
			}
			int depth = 0;
			do {
				depth += at(TokenKind::LBracket) ? 1 : at(TokenKind::RBracket) ? -1 : 0;
				advance();
			} while (depth > 0 && !at(TokenKind::EndOfFile));
		}
		const bool closed = at(TokenKind::RParen);
		if (closed) {
			advance();
		}
		if (!closed || !(at(TokenKind::Comma) || at(TokenKind::RParen))) {
			declares = false;
			break;
		}
	} while (match(TokenKind::Comma));
	_position = start;
	return declares;
}

void Parser::rejectParameterListInitializer(const VariableDeclaration& declaration) {
	if (!at(TokenKind::LParen) || !parensDeclareFunction()) {
		return;
	}
	Type plain = declaration.type;
	plain.isConst = false;
	throw CompileError("\"" + declaration.name + "\" is followed by a parameter list, so C++ and "
		"xcrun metal declare a function here, not a variable (the most vexing parse), and mslc does not "
		"lower block-scope function declarations; write braces, \"" + declaration.name + "{...}\", or "
		"\"= " + (plain.isAuto ? std::string("value") : typeName(plain) + "(...)") + "\" to construct the variable");
}

// The initialiser a local may have after its name: "= value", a braced list,
// or constructor arguments. A const local has to have one.
void Parser::parseLocalInitializer(VariableDeclaration& declaration) {
	rejectParameterListInitializer(declaration);

	if (declaration.type.isAuto) {
		parseAutoInitializer(declaration);
		return;
	}

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

StatementPtr Parser::parseBody(bool sharesEnclosingScope) {
	if (at(TokenKind::LBrace)) {
		return parseCompoundStatement(sharesEnclosingScope);
	}

	const NestingScope scope(*this);
	const LocalScope locals(*this);
	_blocks.back().sharesEnclosingScope = sharesEnclosingScope;
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

StatementPtr Parser::parseCompoundStatement(bool sharesEnclosingScope) {
	const NestingScope scope(*this);
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Compound;
	statement->line = line();
	const LocalScope locals(*this);
	_blocks.back().sharesEnclosingScope = sharesEnclosingScope;

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

	// A GNU statement expression that is a statement of its own, "({ ... });", which Apple's compiler takes as
	// clang does: its value is dropped, so it runs as the block it holds, in a scope of its own, and a break,
	// continue or return in it acts as one in that block would. One in an expression is refused (parsePrimary), even where its value is dropped.
	if (at(TokenKind::LParen) && lookahead().kind == TokenKind::LBrace) {
		size_t depth = 0;
		size_t close = 0;
		for (size_t k = 0;; ++k) {
			const TokenKind tokenKind = lookahead(k).kind;
			if (tokenKind == TokenKind::EndOfFile) {
				break;
			}
			if (tokenKind == TokenKind::LParen || tokenKind == TokenKind::LBrace || tokenKind == TokenKind::LBracket) {
				++depth;
			} else if (tokenKind == TokenKind::RParen || tokenKind == TokenKind::RBrace
				|| tokenKind == TokenKind::RBracket) {
				if (--depth == 0) {
					close = k;
					break;
				}
			}
		}
		if (close && lookahead(close).kind == TokenKind::RParen && lookahead(close - 1).kind == TokenKind::RBrace
			&& lookahead(close + 1).kind == TokenKind::Semicolon) {
			advance();
			StatementPtr block = parseCompoundStatement();
			expect(TokenKind::RParen, "to close a statement expression");
			expect(TokenKind::Semicolon, "after a statement expression");
			return block;
		}
	}

	if (at(TokenKind::Semicolon)) {
		advance();
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
	}

	// parseSwitchStatement takes the labels that sit directly in a switch body, so a label
	// that reaches here is outside any switch or nested in a compound, loop or if within one.
	// The second form (Duff's device) would need OpSwitch to branch into the middle of a
	// structured construct, which SPIR-V does not allow.
	if (atKeyword("case") || atKeyword("default")) {
		const std::string label = atKeyword("case") ? "case" : "default";
		if (_switchDepth == 0) {
			throw CompileError("'" + label + "' label is outside a switch statement");
		}
		throw CompileError("a '" + label + "' label nested inside a compound statement, loop or if within a switch "
			"body is not supported: put every label directly in the switch body");
	}

	if (matchIdentifier("static_assert")) {
		statement->refusal = parseStaticAssert(true);
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
	}

	if (atKeyword("namespace") && lookahead().kind == TokenKind::Identifier && lookahead(2).kind == TokenKind::Assign) {
		advance();
		parseNamespaceAlias();
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

	// "using namespace N;" and "using N::x;" hold for the rest of the block.
	if (matchIdentifier("using")) {
		parseUsing();
		statement->kind = StatementKind::ExpressionStatement;
		return statement;
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

	if (atKeyword("do")) {
		return parseDoWhileStatement();
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
	// Apple also takes [[clang::fallthrough]]; only [[::clang::fallthrough]] is an error there. It
	// accepts further spellings ([[gnu::fallthrough]], [[__fallthrough__]]) that are not recognised here.
	const size_t nameAt = at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket
		&& lookahead(2).kind == TokenKind::Identifier && lookahead(2).text == "clang"
		&& lookahead(3).kind == TokenKind::ColonColon ? 4 : 2;
	if (at(TokenKind::LBracket) && lookahead().kind == TokenKind::LBracket
		&& lookahead(nameAt).kind == TokenKind::Identifier && lookahead(nameAt).text == "fallthrough"
		&& lookahead(nameAt + 1).kind == TokenKind::RBracket && lookahead(nameAt + 2).kind == TokenKind::RBracket) {
		for (size_t i = 0; i < nameAt + 3; ++i) {
			advance();
		}
		expect(TokenKind::Semicolon, "after a [[fallthrough]] attribute");
		if (_switchDepth == 0) {
			throw CompileError("fallthrough annotation is outside switch statement");
		}
		statement->kind = StatementKind::ExpressionStatement;
		statement->isFallthrough = true;
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
		// A local hides a struct tag of its own spelling only where it is not
		// qualified with "::": a bare name that could be the local or the tag is the
		// local, but "A::S" or "::S" still names the struct (j1).
		const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
			&& isLocal(name);
		// "S{1,2}.a = 3;" starts an expression: a declaration names a variable after the type.
		Type typedefOf;
		const bool bracedTemporary = kind() == TokenKind::Identifier && lookahead().kind == TokenKind::LBrace
			&& (_unit.findStruct(name) != nullptr
				|| (resolveTypeName(name, typedefOf) && _unit.findStruct(typedefOf.namedType) != nullptr));
		size_t arrayTokens = 0;
		const bool stdArrayType = (kind() == TokenKind::Identifier || at(TokenKind::ColonColon))
			&& !isLocal(name) && (atStdArray(arrayTokens) || atLibraryVec(arrayTokens));
		const bool looksLikeType = !bracedTemporary &&
			((kind() == TokenKind::Identifier || at(TokenKind::ColonColon)) && !objectHidesTag(name) && (resolveTypeName(name, probe)
				|| isTypeQualifier(name) || isResourceTypeName(name)
				|| (_unit.findStruct(name) != nullptr && !localHidesStruct)))
			|| stdArrayType
			|| (kind() == TokenKind::Identifier && (isIntegerSpecifier(current().text) || current().text == "auto"))
			|| atKeyword("enum") || atKeyword("device") || atKeyword("constant")
			|| atKeyword("threadgroup") || atKeyword("thread");

		if (looksLikeType) {
			statement->kind = StatementKind::DeclarationStatement;
			VariableDeclaration declaration;
			Type base;
			const size_t typeFrom = _position;
			declaration.type = parseType(true, true, &base);
			_localIndependent = typeTokensIndependent(typeFrom);
			_localInjected = namesInjectedClass(typeFrom);
			// An array alias ends in its name; written brackets end in "]".
			if (declaration.type.arrayLength && !declaration.type.isStdArray && _tokens[_position - 1].kind == TokenKind::RBracket) {
				throw CompileError("brackets are not allowed here; write the array length after the name, as float v[N]");
			}
			if (declaration.type.isAuto && declaration.type.isPointer) {
				throw CompileError("a local pointer declared with auto is not lowered yet");
			}
			declaration.isReference = at(TokenKind::Ampersand);
			rejectLocalQualifiers(declaration.type, declaration.isReference, true);
			if (declaration.isReference) {
				advance();
				if (declaration.type.isPointer || (declaration.type.arrayLength && !declaration.type.isStdArray)
					|| declaration.type.resource != ResourceKind::None) {
					throw CompileError("a reference to a pointer, an array or a resource is not lowered yet");
				}
			}

			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
			}
			declaration.name = std::string(advance().text);
			declareLocal(declaration.name);
			noteLocal(declaration.name);

			// "float a[4]" and "float a[] = {...}": the length follows the name.
			bool deduceLength = false;
			if (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
				if (declaration.isReference || declaration.type.isPointer || declaration.type.arrayLength
					|| declaration.type.resource != ResourceKind::None) {
					throw CompileError("an array of references, pointers or resources is not lowered yet");
				}
				if (declaration.type.isAuto) {
					throw CompileError("an array cannot be declared with auto; write the element type");
				}
				deduceLength = parseArrayDimensions(declaration.type);
				if (deduceLength && !at(TokenKind::Assign)) {
					throw CompileError("the array \"" + declaration.name + "\" has no size and needs an initialiser list");
				}
			}

			if (declaration.isReference) {
				if (!match(TokenKind::Assign)) {
					throw CompileError("the reference \"" + declaration.name + "\" needs an initialiser");
				}
				declaration.initializer = parseAssignment();
				recordObjectType(declaration.name, declaration.type);
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
			if (deduceLength) {
				deduceArrayLength(declaration);
			}
			recordConstant(declaration, true);
			recordObjectType(declaration.name, declaration.type);

			// "T a, b = 1, c;" declares each name from the type before the first
			// declarator's stars and brackets, which bind to that name alone.
			while (at(TokenKind::Comma)) {
				if (declaration.type.isAuto) {
					throw CompileError("several declarators after auto are not lowered yet; Apple requires each to "
						"deduce the same type, so declare each variable on its own line");
				}
				advance();
				auto next = std::make_unique<Statement>();
				next->kind = StatementKind::DeclarationStatement;
				next->line = line();
				next->declaration = parseNextLocalDeclarator(base);
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

// A declarator after the first in "T a, *b, c[2] = {...};", after its comma.
VariableDeclaration Parser::parseNextLocalDeclarator(const Type& base) {
	VariableDeclaration another;
	if (at(TokenKind::Ampersand)) {
		throw CompileError("a reference after the first declarator is not lowered yet; "
			"references are not lowered in a declaration with several declarators");
	}
	another.type = base;
	parseDeclaratorStars(another.type);
	if (kind() != TokenKind::Identifier) {
		throw CompileError("expected a variable name, found " + std::string(tokenKindName(kind())));
	}
	another.name = std::string(advance().text);
	declareLocal(another.name);
	noteLocal(another.name);

	bool deduceLength = false;
	if (at(TokenKind::LBracket) && lookahead().kind != TokenKind::LBracket) {
		if (another.type.isPointer) {
			throw CompileError("an array of pointers is not lowered yet");
		}
		deduceLength = parseArrayDimensions(another.type);
	}
	parseLocalInitializer(another);
	if (deduceLength) {
		deduceArrayLength(another);
	}
	recordConstant(another, true);
	recordObjectType(another.name, another.type);
	return another;
}

// A sampler declared in the shader: "constexpr sampler s(options);", with a
// brace list or "= sampler(options)" for the options, or with none. Apple's
// compiler takes the same declaration without constexpr, so this does too.
void Parser::parseSamplerLocal(VariableDeclaration& declaration) {
	const Type& type = declaration.type;
	if (type.resource != ResourceKind::Sampler) {
		if (type.isPointer || type.arrayLength || (type.addressSpace != AddressSpace::None
			&& type.addressSpace != AddressSpace::Thread)) {
			throw CompileError("a local texture \"" + declaration.name + "\" that is a pointer, an array or in an "
				"address space other than thread is not lowered yet");
		}
		// A texture local is another name for a texture the function already names: sema binds it to
		// the same descriptor. Reassigning it, or choosing it at run time, has no such model.
		if (!match(TokenKind::Assign)) {
			throw CompileError("the texture \"" + declaration.name + "\" needs an initialiser that names a "
				"texture, as in \"texture2d<float> b = a;\": a texture local is another name for one, and "
				"mslc does not lower one that is bound later");
		}
		declaration.initializer = parseAssignment();
		expect(TokenKind::Semicolon, "after a declaration");
		return;
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
			// A copy of a sampler: sema binds the name to the same descriptor.
			declaration.initializer = parseAssignment();
			expect(TokenKind::Semicolon, "after a declaration");
			return;
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
	// An axis spelled clamp_to_zero (clamp_to_border is the same mode here): Apple refuses it beside a
	// border color other than transparent black, which indium would apply to it.
	bool zeroSpelled = false;

	while (!at(closing) && !at(TokenKind::EndOfFile)) {
		if (kind() == TokenKind::Identifier && current().text == "metal" && lookahead().kind == TokenKind::ColonColon) {
			advance();
			advance();
		} else if (const size_t prefix = libraryPrefixTokens(); prefix && lookahead(prefix).kind == TokenKind::Identifier) {
			for (size_t i = 0; i < prefix; ++i) {
				advance();
			}
		}
		if (kind() != TokenKind::Identifier) {
			throw CompileError("expected a sampler option, found " + std::string(tokenKindName(kind())));
		}

		const std::string family(advance().text);
		static const std::set<std::string> unlowered = {
			"reduction",
		};
		static const std::set<std::string> known = {
			"coord", "address", "s_address", "t_address", "r_address", "filter",
			"mag_filter", "min_filter", "mip_filter", "compare_func", "max_anisotropy", "lod_clamp", "border_color",
		};
		if (unlowered.count(family)) {
			throw CompileError("the sampler option \"" + family + "\" is accepted by Apple but not lowered: Vulkan "
				"selects it with VkSamplerReductionMode (VK_EXT_sampler_filter_minmax) and the reflection's sampler "
				"has no field for it, so it would be dropped silently; mslc lowers "
				"coord, address, s_address, t_address, r_address, filter, mag_filter, min_filter, "
				"mip_filter, compare_func, max_anisotropy, lod_clamp and border_color");
		}
		if (!known.count(family)) {
			throw CompileError("\"" + family + "\" is not a sampler option");
		}

		if (family == "max_anisotropy" || family == "lod_clamp") {
			// A call-like option: a number, or for lod_clamp the lower and upper bound.
			expect(TokenKind::LParen, ("after \"" + family + "\"").c_str());
			const auto number = [&]() {
				const bool negative = match(TokenKind::Minus);
				if (kind() != TokenKind::IntegerLiteral && kind() != TokenKind::FloatLiteral) {
					throw CompileError("\"" + family + "\" takes number literals");
				}
				const double value = kind() == TokenKind::IntegerLiteral ? static_cast<double>(current().integerValue)
					: current().floatValue;
				advance();
				if (!std::isfinite(value) || std::fabs(value) > std::numeric_limits<float>::max()) {
					throw CompileError("\"" + family + "\" takes a finite float");
				}
				return negative ? -value : value;
			};
			if (family == "max_anisotropy") {
				const double value = number();
				if (value < 1 || value > 16 || value != static_cast<uint32_t>(value)) {
					throw CompileError("max_anisotropy takes an integer from 1 to 16");
				}
				if (assigned.insert("anisotropy").second) {
					state.anisotropy = static_cast<uint32_t>(value);
				}
			} else {
				const double low = number();
				expect(TokenKind::Comma, "between the bounds of lod_clamp");
				const double high = number();
				if (low < 0 || high < low) {
					throw CompileError("lod_clamp takes a lower bound of at least 0 and an upper bound not below it");
				}
				if (assigned.insert("lod").second) {
					state.lodMin = static_cast<float>(low);
					state.lodMax = static_cast<float>(high);
				}
			}
			expect(TokenKind::RParen, ("to close \"" + family + "\"").c_str());
			if (!match(TokenKind::Comma)) {
				break;
			}
			if (at(closing)) {
				throw CompileError("expected a sampler option after ',' in a sampler's option list");
			}
			continue;
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
			if (value == "bicubic") {
				throw CompileError("\"" + spelled + "\" is accepted by Apple but not lowered: Vulkan has bicubic "
					"filtering only as VK_EXT_filter_cubic / VK_IMG_filter_cubic, outside core, and the reflection's "
					"sampler has no field for it, so it would be dropped silently");
			}
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
					zeroSpelled = zeroSpelled || value == "clamp_to_zero";
				});
		} else if (family == "filter" || family == "mag_filter" || family == "min_filter") {
			const SamplerFilter chosen = filter();
			fill(family == "filter" ? std::initializer_list<const char*>{ "mag", "min" }
					: family == "mag_filter" ? std::initializer_list<const char*>{ "mag" }
					: std::initializer_list<const char*>{ "min" },
				[&](const char* attribute) {
					(attribute[1] == 'a' ? state.magFilter : state.minFilter) = chosen;
				});
		} else if (family == "border_color") {
			static const std::map<std::string_view, SamplerBorder> borders = {
				{ "transparent_black", SamplerBorder::TransparentBlack },
				{ "opaque_black", SamplerBorder::OpaqueBlack }, { "opaque_white", SamplerBorder::OpaqueWhite },
			};
			const auto chosen = borders.find(value);
			if (chosen == borders.end()) {
				throw CompileError("\"" + spelled + "\" is not a border color");
			}
			fill({ "border" }, [&](const char*) { state.border = chosen->second; });
		} else if (family == "compare_func") {
			static const std::map<std::string_view, SamplerCompare> compares = {
				{ "never", SamplerCompare::Never }, { "less", SamplerCompare::Less },
				{ "less_equal", SamplerCompare::LessEqual }, { "greater", SamplerCompare::Greater },
				{ "greater_equal", SamplerCompare::GreaterEqual }, { "equal", SamplerCompare::Equal },
				{ "not_equal", SamplerCompare::NotEqual }, { "always", SamplerCompare::Always },
			};
			const auto chosen = compares.find(value);
			if (chosen == compares.end()) {
				throw CompileError("\"" + spelled + "\" is not a comparison function");
			}
			fill({ "compare" }, [&](const char*) { state.compare = chosen->second; });
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

	if (zeroSpelled && state.border != SamplerBorder::TransparentBlack) {
		throw CompileError("invalid values combination in sampler initialization: clamp_to_zero takes no "
			"border_color other than transparent_black");
	}
	if (!state.normalizedCoordinates) {
		if (state.compare != SamplerCompare::Never) {
			throw CompileError("invalid values combination in sampler initialization: with coord::pixel the "
				"compare_func has to be never");
		}
		const auto clamps = [](SamplerAddress mode) {
			return mode == SamplerAddress::ClampToZero || mode == SamplerAddress::ClampToEdge;
		};
		if (assigned.count("anisotropy") || assigned.count("lod")) {
			throw CompileError("invalid values combination in sampler initialization: with coord::pixel, Vulkan "
				"needs anisotropy off and no lod range, so max_anisotropy and lod_clamp are refused");
		}
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

	std::vector<StatementPtr> extraDeclarators;
	{
		Type probe;
		size_t nameTokens = 0;
		const std::string name = kind() == TokenKind::Identifier || at(TokenKind::ColonColon)
			? peekResolved(nameTokens) : std::string();
		// A local hides a struct tag of its own spelling only where it is not
		// qualified with "::" (j8, j9; see parseStatement's identical guard).
		const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
			&& isLocal(name);
		size_t vecTokens = 0;
		const bool looksLikeType = (kind() == TokenKind::Identifier
				&& (isIntegerSpecifier(current().text) || current().text == "auto"))
			|| ((kind() == TokenKind::Identifier || at(TokenKind::ColonColon))
				&& !objectHidesTag(name) && (resolveTypeName(name, probe) || (_unit.findStruct(name) != nullptr && !localHidesStruct)))
			|| atLibraryVec(vecTokens);

		if (looksLikeType) {
			VariableDeclaration declaration;
			Type base;
			const size_t typeFrom = _position;
			declaration.type = parseType(false, true, &base);
			_localIndependent = typeTokensIndependent(typeFrom);
			_localInjected = namesInjectedClass(typeFrom);
			rejectLocalQualifiers(declaration.type, at(TokenKind::Ampersand));
			if (declaration.type.isAuto && (declaration.type.isPointer || declaration.type.arrayLength)) {
				throw CompileError("a pointer or an array declared with auto is not lowered yet");
			}
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a loop variable name in a for initialiser");
			}
			declaration.name = std::string(advance().text);
			declareLocal(declaration.name);
			noteLocal(declaration.name);
			if (declaration.type.isAuto) {
				rejectParameterListInitializer(declaration);
				parseAutoInitializer(declaration);
			} else if (match(TokenKind::Assign)) {
				declaration.initializer = parseInitializer();
			} else if (at(TokenKind::LBrace)) {
				declaration.initializer = parseInitializerList();
			}
			recordObjectType(declaration.name, declaration.type);
			statement->forInitializer = std::move(declaration);

			// "for (int i = 0, j = 1; ...)" : the further declarators are declared
			// ahead of the loop, in a block of its own, so the names stay its own.
			while (at(TokenKind::Comma)) {
				if (base.isAuto) {
					throw CompileError("several declarators after auto are not lowered yet; Apple requires each "
						"to deduce the same type, so declare each variable on its own line");
				}
				advance();
				auto next = std::make_unique<Statement>();
				next->kind = StatementKind::DeclarationStatement;
				next->line = line();
				next->declaration = parseNextLocalDeclarator(base);
				extraDeclarators.push_back(std::move(next));
			}
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

	statement->forBody = parseBody(true);

	if (extraDeclarators.empty()) {
		return statement;
	}

	auto block = std::make_unique<Statement>();
	block->kind = StatementKind::Compound;
	block->line = statement->line;
	auto first = std::make_unique<Statement>();
	first->kind = StatementKind::DeclarationStatement;
	first->line = statement->line;
	first->declaration = std::move(statement->forInitializer);
	statement->forInitializer.reset();
	block->children.push_back(std::move(first));
	for (StatementPtr& extra: extraDeclarators) {
		block->children.push_back(std::move(extra));
	}
	block->children.push_back(std::move(statement));
	return block;
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

StatementPtr Parser::parseDoWhileStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::While;
	statement->isDoWhile = true;
	statement->line = line();

	expectKeyword("do", "at the start of a do statement");
	statement->whileBody = parseBody();
	expectKeyword("while", "after the body of a do statement");
	expect(TokenKind::LParen, "after 'while'");
	statement->whileCondition = parseExpression();
	expect(TokenKind::RParen, "after a do-while condition");
	expect(TokenKind::Semicolon, "after a do-while statement");

	return statement;
}

// A statement that does nothing: a null statement or a block of them.
static bool isEmptyStatement(const Statement& statement) {
	if (statement.kind == StatementKind::ExpressionStatement) {
		return !statement.expression && !statement.isFallthrough;
	}
	return statement.kind == StatementKind::Compound
		&& std::all_of(statement.children.begin(), statement.children.end(),
			[](const StatementPtr& child) { return isEmptyStatement(*child); });
}

// Control never reaches the statement after one of these, and Apple ignores a marker there
// (discard_fragment() does not count: Apple still checks what follows it).
static bool leavesTheBlock(const Statement& statement) {
	return statement.kind == StatementKind::Return || statement.kind == StatementKind::Break
		|| statement.kind == StatementKind::Continue;
}

[[noreturn]] static void rejectFallthroughPlacement() {
	throw CompileError("fallthrough annotation does not directly precede switch label");
}

// Whether a [[fallthrough]]; is anywhere in this statement, reachable or not.
static bool containsFallthrough(const Statement& statement) {
	switch (statement.kind) {
		case StatementKind::ExpressionStatement:
			return statement.isFallthrough;
		case StatementKind::Compound:
			return std::any_of(statement.children.begin(), statement.children.end(),
				[](const StatementPtr& child) { return containsFallthrough(*child); });
		case StatementKind::If:
			return containsFallthrough(*statement.thenBranch)
				|| (statement.elseBranch && containsFallthrough(*statement.elseBranch));
		case StatementKind::For:
			return containsFallthrough(*statement.forBody);
		case StatementKind::While:
			return containsFallthrough(*statement.whileBody);
		case StatementKind::Switch:
			return statement.switchFallsOut;
		default:
			return false;
	}
}

// What the statements after a jump have shown so far: a marker was seen (only the last marker
// of the run is ignored), and a marker was seen on the way to here (so what follows must flow
// plainly on to the label).
struct UnreachableRun {
	bool seenMarker = false;
	bool afterMarker = false;
};

// A statement after a jump. Apple ignores a marker there, except that only the last marker of
// such a run is ignored, it still has to reach the label through plain statements (no jump,
// loop, switch or if with an else after it), and a marker in a loop body is an error whether it
// is reached or not.
static void checkUnreachable(const Statement& statement, UnreachableRun& run) {
	const bool branches = statement.kind == StatementKind::For || statement.kind == StatementKind::While
		|| statement.kind == StatementKind::Switch
		|| (statement.kind == StatementKind::If && statement.elseBranch);
	if (run.afterMarker && (branches || leavesTheBlock(statement))) rejectFallthroughPlacement();
	switch (statement.kind) {
		case StatementKind::ExpressionStatement:
			if (statement.isFallthrough) {
				if (run.seenMarker) rejectFallthroughPlacement();
				run.seenMarker = run.afterMarker = true;
			}
			return;
		case StatementKind::Compound:
			for (const StatementPtr& child: statement.children) checkUnreachable(*child, run);
			return;
		case StatementKind::If: {
			// A jump in a branch leaves only that branch, so a branch starts without the
			// "after a marker" demand and the if as a whole carries a marker out of either.
			UnreachableRun inThen{ run.seenMarker, false };
			UnreachableRun inElse{ run.seenMarker, false };
			checkUnreachable(*statement.thenBranch, inThen);
			if (statement.elseBranch) checkUnreachable(*statement.elseBranch, inElse);
			run.seenMarker = inThen.seenMarker || inElse.seenMarker;
			run.afterMarker = run.afterMarker || inThen.afterMarker || inElse.afterMarker;
			return;
		}
		default:
			if (containsFallthrough(statement)) rejectFallthroughPlacement();
			return;
	}
}

// Whether a [[fallthrough]]; inside this statement is the last thing it does, so that
// control leaves the statement straight from the marker. A marker anywhere else, or in a
// loop (whose body goes back to the head, not on to a label), is an error.
static bool endsWithFallthrough(const Statement& statement) {
	const auto reject = rejectFallthroughPlacement;
	switch (statement.kind) {
		case StatementKind::ExpressionStatement:
			return statement.isFallthrough;
		case StatementKind::Compound: {
			bool ends = false;
			UnreachableRun run;
			bool jumped = false;
			for (const StatementPtr& child: statement.children) {
				if (jumped) {
					checkUnreachable(*child, run);
					continue;
				}
				if (ends && !isEmptyStatement(*child)) reject();
				if (leavesTheBlock(*child)) {
					jumped = true;
					continue;
				}
				ends = ends || endsWithFallthrough(*child);
			}
			return ends;
		}
		case StatementKind::If: {
			const bool inThen = endsWithFallthrough(*statement.thenBranch);
			const bool inElse = statement.elseBranch && endsWithFallthrough(*statement.elseBranch);
			return inThen || inElse;
		}
		case StatementKind::For:
			if (containsFallthrough(*statement.forBody)) reject();
			return false;
		case StatementKind::While:
			if (containsFallthrough(*statement.whileBody)) reject();
			return false;
		case StatementKind::Switch:
			// A marker ending the last case of this switch falls out of it.
			return statement.switchFallsOut;
		default:
			return false;
	}
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

	struct DepthGuard {
		int& depth;
		explicit DepthGuard(int& value): depth(value) { ++depth; }
		~DepthGuard() { --depth; }
	} depthGuard(_switchDepth);

	SwitchCase* kase = nullptr;
	bool markerPending = false;
	bool unreachable = false;
	UnreachableRun unreachableRun;
	while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
		if (atKeyword("case")) {
			advance();
			SwitchCase next;
			next.line = line();
			next.value = parseAssignment();
			expect(TokenKind::Colon, "after a case value");
			statement->switchCases.push_back(std::move(next));
			kase = &statement->switchCases.back();
			markerPending = false;
			unreachable = false;
			unreachableRun = UnreachableRun();
			continue;
		}
		if (atKeyword("default")) {
			advance();
			SwitchCase next;
			next.line = line();
			expect(TokenKind::Colon, "after 'default'");
			statement->switchCases.push_back(std::move(next));
			kase = &statement->switchCases.back();
			markerPending = false;
			unreachable = false;
			unreachableRun = UnreachableRun();
			continue;
		}

		// A [[fallthrough]]; has to reach the next label with nothing in between, as Apple
		// requires: it may end a nested block or an if branch, but not be followed by a
		// statement, and a loop body never reaches a label. Null statements and empty
		// blocks do not count.
		std::vector<StatementPtr>& into = kase ? kase->body : statement->switchPreamble;
		const size_t before = into.size();
		appendStatement(into);
		for (size_t index = before; index < into.size(); ++index) {
			const Statement& added = *into[index];
			// Nothing after a jump, and nothing before the first label, is reached.
			if (!kase) continue;
			if (unreachable) {
				checkUnreachable(added, unreachableRun);
				continue;
			}
			if (markerPending && !isEmptyStatement(added)) {
				throw CompileError("fallthrough annotation does not directly precede switch label");
			}
			if (leavesTheBlock(added)) {
				unreachable = true;
				continue;
			}
			markerPending = markerPending || endsWithFallthrough(added);
		}
	}

	// In a nested switch the marker falls out to whatever follows the switch, which the
	// enclosing switch checks; in the outermost one no label can follow it.
	if (markerPending && _switchDepth == 1) {
		throw CompileError("fallthrough annotation does not directly precede switch label");
	}
	statement->switchFallsOut = markerPending;
	expect(TokenKind::RBrace, "to close a switch body");

	return statement;
}

StatementPtr Parser::parseReturnStatement() {
	auto statement = std::make_unique<Statement>();
	statement->kind = StatementKind::Return;
	statement->line = line();

	expectKeyword("return", "at the start of a return");

	if (!at(TokenKind::Semicolon)) {
		// "return {1, 2, 3};" copy-initialises the returned value from a braced list.
		statement->expression = at(TokenKind::LBrace) ? parseInitializerList() : parseExpression();
	}

	expect(TokenKind::Semicolon, "after a return");

	return statement;
}

ExpressionPtr Parser::parseExpression() {
	auto left = parseAssignment();
	while (at(TokenKind::Comma)) {
		advance();
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Comma;
		expression->line = left->line;
		expression->left = std::move(left);
		expression->right = parseAssignment();
		measure(*expression);
		left = std::move(expression);
	}
	return left;
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
		// "a = {1, 2, 3};" assigns a braced list, which the emitter takes for an array<T, N> only.
		expression->right = !compound && at(TokenKind::LBrace) ? parseInitializerList() : parseAssignment();
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

// Inside the angle brackets of a trait's argument a ">" closes the list, so it is no operator there
// (C++ reads it the same way); a parenthesised operand is a fresh expression.
ExpressionPtr Parser::parseRelational() {
	if (_noGreater) {
		return parseBinaryLevel([](Parser& p) { return p.parseShift(); }, { { TokenKind::Less, BinaryOperator::Less },
			{ TokenKind::LessEqual, BinaryOperator::LessEqual }, { TokenKind::GreaterEqual, BinaryOperator::GreaterEqual } });
	}
	return parseBinaryLevel([](Parser& p) { return p.parseShift(); }, { { TokenKind::Less, BinaryOperator::Less }, { TokenKind::LessEqual, BinaryOperator::LessEqual },
		  { TokenKind::Greater, BinaryOperator::Greater }, { TokenKind::GreaterEqual, BinaryOperator::GreaterEqual } });
}

ExpressionPtr Parser::parseShift() {
	if (_noGreater) {
		return parseBinaryLevel([](Parser& p) { return p.parseAdditive(); }, { { TokenKind::ShiftLeft, BinaryOperator::ShiftLeft } });
	}
	return parseBinaryLevel([](Parser& p) { return p.parseAdditive(); }, { { TokenKind::ShiftLeft, BinaryOperator::ShiftLeft }, { TokenKind::ShiftRight, BinaryOperator::ShiftRight } });
}

ExpressionPtr Parser::parseAdditive() {
	return parseBinaryLevel([](Parser& p) { return p.parseMultiplicative(); }, { { TokenKind::Plus, BinaryOperator::Add }, { TokenKind::Minus, BinaryOperator::Subtract } });
}

ExpressionPtr Parser::parseMultiplicative() {
	return parseBinaryLevel([](Parser& p) { return p.parseUnary(); }, { { TokenKind::Star, BinaryOperator::Multiply }, { TokenKind::Slash, BinaryOperator::Divide },
		  { TokenKind::Percent, BinaryOperator::Modulo } });
}

// sizeof(type) is the type's size under Metal's layout, folded here to a ulong literal so every
// later stage sees a constant. A bare type name and a parenthesised expression both start with
// "(", so a type is taken only where one can begin and ")" follows it.
ExpressionPtr Parser::parseSizeof() {
	const size_t startLine = line();
	advance();

	if (at(TokenKind::LParen)) {
		const size_t open = _position;
		advance();

		size_t nameTokens = 0;
		Type probe;
		const std::string name = kind() == TokenKind::Identifier || at(TokenKind::ColonColon)
			? peekResolved(nameTokens) : std::string();
		const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
			&& isLocal(name);
		const bool inOwnBody = _structBody && name == _structBody->name;
		const bool startsType = inOwnBody || (kind() == TokenKind::Identifier && isIntegerSpecifier(current().text))
			|| ((kind() == TokenKind::Identifier || at(TokenKind::ColonColon)) && !objectHidesTag(name)
				&& !(kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon && memberHidesName(name))
				&& (resolveTypeName(name, probe) || isTypeQualifier(name) || isResourceTypeName(name)
					|| (_unit.findStruct(name) != nullptr && !localHidesStruct) || (!localHidesStruct && (atStdArray(nameTokens) || atLibraryVec(nameTokens)))))
			|| atKeyword("device") || atKeyword("constant") || atKeyword("threadgroup") || atKeyword("thread");
		if (startsType) {
			Type type = parseType(true, false);
			// parseType reads the first array length only; float[2][3] has more.
			while (type.arrayLength && at(TokenKind::LBracket)) {
				advance();
				const int64_t length = parseConstantIndex("an array length");
				if (length <= 0) {
					throw CompileError("zero-length arrays are not permitted in C++");
				}
				type.innerArrayLengths.push_back(static_cast<uint32_t>(length));
				expect(TokenKind::RBracket, "to close an array length");
			}
			if (at(TokenKind::RParen)) {
				advance();
				if (_structBody && _structBody->name == type.namedType && !type.isPointer) {
					throw CompileError("sizeof of the struct \"" + type.namedType + "\" inside its own body, "
						"where it is incomplete");
				}
				auto expression = std::make_unique<Expression>();
				expression->kind = ExpressionKind::IntLiteral;
				expression->line = startLine;
				expression->intKind = ScalarKind::ULong;
				expression->isSizeof = true;
				if (isDeductionMarker(type) && !type.isPointer && _inEnableIf) {
					// An enable_if condition read before T is deduced: the read with T decides.
					_dependentCondition = true;
					expression->intValue = 1;
					return expression;
				}
				expression->intValue = sizeofType(type, _unit);
				return expression;
			}
		}
		_position = open;
	}

	auto expression = std::make_unique<Expression>();
	expression->kind = ExpressionKind::Sizeof;
	expression->line = startLine;
	const NestingScope scope(*this);
	expression->left = parseUnary();
	// A declared name is sized here, where the scope that decides which declaration it finds is open.
	if (const std::optional<Type> declared = declaredObjectType(*expression->left)) {
		expression->kind = ExpressionKind::IntLiteral;
		expression->intKind = ScalarKind::ULong;
		expression->isSizeof = true;
		expression->intValue = sizeofType(*declared, _unit);
		expression->left.reset();
		return expression;
	}
	measure(*expression);
	return expression;
}

ExpressionPtr Parser::parseUnary() {
	if (atKeyword("sizeof")) {
		return parseSizeof();
	}

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
		// The address of an element is not a value mslc holds; the atomic_*_explicit
		// functions take one, and the emitter refuses it anywhere else.
		auto address = std::make_unique<Expression>();
		address->kind = ExpressionKind::Unary;
		address->unaryOperator = UnaryOperator::AddressOf;
		address->line = line();
		advance();
		const NestingScope scope(*this);
		address->left = parseUnary();
		measure(*address);
		return address;
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
		// A cast to void evaluates the operand and discards it; the empty type stands
		// for void, which is what a Type holds until something sets it.
		if (head == "void") {
			nameTokens = 1;
			return Type();
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
// enum or a struct, is the functional cast T(operand). Returns null when the
// parenthesis opens an expression, which includes one that starts with a functional
// cast, "(int(x) * 2)": a type name followed by anything but ")" is not a C-style cast.
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
			const size_t open = _position;
			advance();
			Type type;
			parseSpecifierRun(type);
			if (at(TokenKind::Star) || at(TokenKind::Ampersand) || at(TokenKind::Less)) {
				throw CompileError("a cast to a pointer, reference or template type is not supported");
			}
			// "(int(x) * 2)": the type name starts a functional cast inside a parenthesised
			// expression, not a C-style cast, so the parenthesis opens an expression.
			if (!at(TokenKind::RParen)) {
				_position = open;
				return nullptr;
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
	// (vec<T, N>)x; a parenthesis holding more than the type, "(vec<float, 4>(a).x)", is an expression.
	if (size_t vecTokens = 0; atLibraryVec(vecTokens)) {
		const size_t atLine = line();
		Type target;
		try {
			target = parseType(false, false);
		} catch (...) {
			_position = open;
			throw;
		}
		if (!at(TokenKind::RParen)) {
			_position = open;
			return nullptr;
		}
		advance();
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Construct;
		expression->line = atLine;
		expression->constructType = target;
		const NestingScope scope(*this);
		expression->arguments.push_back(parseUnary());
		measure(*expression);
		return expression;
	}
	size_t nameTokens = 0;
	std::optional<Type> type;
	try {
		type = peekCastTargetName(nameTokens);
		// A variable or function of the name hides the struct or enum tag, so "(S) - 1" is
		// S minus one and not a cast ([basic.scope.hiding]); parseSizeof reads the same rule.
		if (type && (kind() == TokenKind::Identifier || at(TokenKind::ColonColon))) {
			size_t spelledTokens = 0;
			const std::string spelled = peekResolved(spelledTokens);
			Type probe;
			const bool localHidesStruct = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
				&& isLocal(spelled) && !resolveTypeName(spelled, probe);
			const bool memberHides = kind() == TokenKind::Identifier && lookahead().kind != TokenKind::ColonColon
				&& memberHidesName(spelled);
			if (objectHidesTag(spelled) || memberHides || localHidesStruct) {
				type.reset();
			}
		}
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

std::optional<Type> Parser::declaredPartType(const Expression& operand) const {
	if (operand.kind == ExpressionKind::Index && !operand.isDereference) {
		std::optional<Type> base = declaredPartType(*operand.left);
		if (!base || (base->isPointer && base->arrayLength)) {
			return std::nullopt;
		}
		if (base->arrayLength) {
			base->dropOuterDimension();
		} else if (base->isPointer) {
			base->isPointer = false;
			base->dropOuterDimension();
		} else {
			return std::nullopt;
		}
		return base;
	}
	if (operand.kind != ExpressionKind::Member) {
		return declaredObjectType(operand);
	}
	const std::optional<Type> owner = declaredPartType(*operand.left);
	if (!owner || owner->arrayLength || owner->namedType.empty() || owner->isPointer != operand.isArrow) {
		return std::nullopt;
	}
	const StructDecl* decl = _unit.findStruct(owner->namedType);
	if (!decl) {
		return std::nullopt;
	}
	for (const StructField& field: decl->fields) {
		if (field.name == operand.memberName) {
			return field.type;
		}
	}
	return std::nullopt;
}

ExpressionPtr Parser::foldStdArrayCall(ExpressionPtr& receiver) {
	if (receiver->kind != ExpressionKind::Member || receiver->isArrow || lookahead().kind != TokenKind::RParen) {
		return nullptr;
	}
	const std::string& name = receiver->memberName;
	if (name != "size" && name != "max_size" && name != "empty" && name != "front" && name != "back") {
		return nullptr;
	}
	const std::optional<Type> declared = declaredPartType(*receiver->left);
	if (!declared || !declared->isStdArray || !declared->arrayLength || declared->isPointer) {
		return nullptr;
	}
	const size_t atLine = receiver->line;
	advance();
	advance();
	auto folded = std::make_unique<Expression>();
	folded->line = atLine;
	if (name == "empty") {
		folded->kind = ExpressionKind::BoolLiteral;
		folded->boolValue = false;
		return folded;
	}
	folded->kind = ExpressionKind::IntLiteral;
	folded->intKind = ScalarKind::ULong;
	folded->isSizeof = true;
	folded->intValue = name == "back" ? *declared->arrayLength - 1 : *declared->arrayLength;
	if (name == "size" || name == "max_size") {
		return folded;
	}
	// front() and back() are the elements themselves, as places: a[0] and a[N - 1].
	folded->intValue = name == "back" ? *declared->arrayLength - 1 : 0;
	folded->isSizeof = false;
	folded->intKind = ScalarKind::Int;
	auto index = std::make_unique<Expression>();
	index->kind = ExpressionKind::Index;
	index->line = atLine;
	index->left = std::move(receiver->left);
	index->arguments.push_back(std::move(folded));
	measure(*index);
	return index;
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
			// "x.template f<int>(a)" names a member template; C++ requires the keyword when x depends on a
			// template parameter, and Apple takes it anywhere the name is one.
			const bool keyword = atKeyword("template") && lookahead().kind == TokenKind::Identifier;
			if (keyword) {
				advance();
			}
			if (atKeyword("operator")) {
				throw CompileError("calling an operator with member syntax is not lowered; write s[i]");
			}
			if (kind() != TokenKind::Identifier) {
				throw CompileError("expected a member name after '" + op + "'");
			}
			auto member = std::make_unique<Expression>();
			member->kind = ExpressionKind::Member;
			member->isArrow = op == "->";
			member->line = expression->line;
			member->left = std::move(expression);
			member->memberName = std::string(advance().text);
			parseMemberTemplateArguments(*member, keyword);
			measure(*member);
			expression = std::move(member);
			continue;
		}

		if (at(TokenKind::LParen)) {
			if (ExpressionPtr folded = foldStdArrayCall(expression)) {
				expression = std::move(folded);
				continue;
			}
			const auto hasName = [&](const std::set<std::string>& set) {
				return set.count(expression->name) || std::any_of(expression->overloadNames.begin(),
					expression->overloadNames.end(), [&](const std::string& n) { return set.count(n) != 0; });
			};
			if (expression->kind == ExpressionKind::Identifier && !expression->hasTemplateArguments
				&& (expression->fileScope || !isLocal(expression->name))
				&& hasName(_entryTemplateNames) && !hasName(_unit.functionTemplates) && !hasName(_helperNames)) {
				throw CompileError("cannot call the entry point \"" + expression->name + "\"");
			}
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

// "<float, 3>" after the name of a member function template, in a call: the "<" is the current token. Only a
// name some struct declares as a member template is tried, and only a list closed by ">(" is taken, so a
// comparison of a field of that name is left alone.
void Parser::parseMemberTemplateArguments(Expression& member, bool keyword) {
	member.templateKeyword = keyword;
	if (keyword && !_memberTemplateNames.count(member.memberName)) {
		throw CompileError("\"" + member.memberName + "\" follows the 'template' keyword, and no struct declares a "
			"member function template of that name");
	}
	// "x.template f(a)" leaves the arguments to deduction: with no "<" the call below is read as any other.
	if (!at(TokenKind::Less) || !_memberTemplateNames.count(member.memberName)) {
		return;
	}
	const size_t saved = _position;
	bool taken = false;
	try {
		advance();
		std::vector<TemplateArgument> arguments;
		while (!at(TokenKind::Greater)) {
			arguments.push_back(parseTemplateArgument());
			if (!match(TokenKind::Comma)) {
				break;
			}
		}
		if (at(TokenKind::Greater) && lookahead().kind == TokenKind::LParen) {
			advance();
			member.templateArguments = std::move(arguments);
			taken = true;
		}
	} catch (const ClassTemplateError&) {
		throw;
	} catch (const CompileError&) {
	}
	// Inside a template body Apple rejects "x.f<int>(a)" when x depends on a template parameter, and takes it when
	// x does not. mslc takes it for "this" and for a local or parameter whose declared type names no template
	// parameter; any other receiver is taken as dependent, which over-refuses a few valid sources loudly.
	if (taken && !keyword && (_classDepth > 0 || _inInstance) && !receiverIndependent(*member.left)) {
		throw CompileError("explicit template arguments on \"" + member.memberName + "\" inside a template need the "
			"keyword unless the receiver is known not to depend on a template parameter: write \".template "
			+ member.memberName + "<...>(...)\" (Apple requires it when the receiver depends on one; mslc takes the "
			"bare form only where the receiver's type provably names no template parameter, so a dependent "
			"receiver, an auto local or a receiver inside a class template that is not a concrete local needs the "
			"keyword)");
	}
	if (taken) {
		return;
	}
	if (keyword) {
		throw CompileError("\"template " + member.memberName + "\" is followed by \"<\", which is not a list of "
			"explicit template arguments closed by \">(\"");
	}
	_position = saved;
}

std::vector<ExpressionPtr> Parser::parseArgumentList(const char* closing) {
	std::vector<ExpressionPtr> arguments;

	while (!at(TokenKind::RParen) && !at(TokenKind::EndOfFile)) {
		arguments.push_back(parseAssignment());

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
	// Deliberate limit (known-issues.md): char pointers, char arrays and
	// bool-from-pointer have no lowered representation.
	static const char* const kStringLimit = "mslc has no string type, so char pointers, "
		"char arrays and bool-from-pointer are not lowered";
	if (at(TokenKind::StringLiteral)) {
		throw CompileError(std::string("a string literal is not lowered; ") + kStringLimit);
	}

	// L"x", u8"x", R"(x)": the lexer reads the prefix as an identifier glued to a string.
	if (kind() == TokenKind::Identifier && lookahead().kind == TokenKind::StringLiteral
		&& current().text.data() + current().text.size() == lookahead().text.data()) {
		static const std::string_view prefixes[] = {"L", "u", "U", "u8", "R", "LR", "uR", "UR", "u8R"};
		for (const std::string_view prefix : prefixes) {
			if (current().text == prefix) {
				throw CompileError("a prefixed string literal is not lowered; " + std::string(kStringLimit));
			}
		}
	}

	if (ExpressionPtr trait = tryParseTraitValue()) {
		return trait;
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
		if (lookahead().kind == TokenKind::LBrace) {
			throw CompileError("a statement expression \"({ ... })\" is lowered only as a statement of its own, "
				"\"({ ... });\"; one in an expression is not lowered");
		}
		advance();
		const RestoreFlag freshExpression(_noGreater, false);
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

	if (size_t arrayTokens = 0; (kind() == TokenKind::Identifier || at(TokenKind::ColonColon))
		&& !isLocal(std::string(current().text)) && atStdArray(arrayTokens)) {
		const size_t atLine = line();
		const Type type = parseType(false, false);
		if (!at(TokenKind::LBrace)) {
			throw CompileError("array<T, N> in an expression is lowered as a braced list, array<T, N>{...}");
		}
		ExpressionPtr list = parseInitializerList();
		list->constructType = type;
		list->line = atLine;
		return list;
	}

	if (size_t asTypeTokens = 0; atAsType(asTypeTokens)) {
		return parseAsType(asTypeTokens);
	}

	// vec<T, N>(...) constructs the vector, as float4(...) does.
	if (size_t vecTokens = 0; (kind() == TokenKind::Identifier || at(TokenKind::ColonColon))
		&& !isLocal(std::string(current().text)) && atLibraryVec(vecTokens)) {
		const size_t atLine = line();
		const Type type = parseType(false, false);
		if (!at(TokenKind::LParen)) {
			throw CompileError("expected '(' after type \"" + typeName(type) + "\"");
		}
		advance();
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Construct;
		expression->line = atLine;
		expression->constructType = type;
		expression->arguments = parseArgumentList("to close a constructor's argument list");
		measure(*expression);
		return expression;
	}

	if (kind() == TokenKind::Identifier || at(TokenKind::ColonColon)) {
		// A name written with namespaces is read whole; a plain one is looked up
		// unless a parameter or local of that name is in scope.
		size_t spelledTokens = 1;
		QualifiedName spelled;
		// "::k" skips the local k: the identifier carries that, since both are named k.
		const bool fileScoped = peekQualifiedName(spelled) && spelled.global && spelled.parts.size() == 1
			&& isLocal(spelled.parts[0]);
		if (_memberFields && kind() == TokenKind::Identifier && !blockName(std::string(current().text))
			&& _memberFields->count(std::string(current().text)) && isLocal("this")
			&& lookahead().kind != TokenKind::ColonColon) {
			const size_t atLine = line();
			auto member = makeThisMember(std::string(advance().text), atLine);
			return member;
		}
		// A member function named bare calls that function on this, or the static
		// one of the struct.
		if (_memberFields && kind() == TokenKind::Identifier && !blockName(std::string(current().text))
			&& (lookahead().kind == TokenKind::LParen
				|| (lookahead().kind == TokenKind::Less && _memberTemplateNames.count(std::string(current().text))))) {
			const auto function = _memberFunctions.find(std::string(current().text));
			if (function != _memberFunctions.end()) {
				const size_t atLine = line();
				const std::string name(advance().text);
				if (function->second) {
					ExpressionPtr staticCall = makeIdentifier(_memberStruct + "::" + name, atLine);
					if (at(TokenKind::Less)) {
						advance();
						staticCall->hasTemplateArguments = true;
						while (!at(TokenKind::Greater)) {
							staticCall->templateArguments.push_back(parseTemplateArgument());
							if (!match(TokenKind::Comma)) {
								break;
							}
						}
						expect(TokenKind::Greater, "to close the template arguments");
					}
					return staticCall;
				}
				ExpressionPtr member = makeThisMember(name, atLine);
				parseMemberTemplateArguments(*member);
				return member;
			}
		}
		// A local hides a namespace's name only where it is not followed by "::".
		std::vector<std::string> alternatives;
		const std::string resolved = kind() == TokenKind::Identifier && isLocal(std::string(current().text))
			&& lookahead().kind != TokenKind::ColonColon ? std::string(current().text)
			: peekResolved(spelledTokens, &alternatives);
		const auto consumeName = [&] {
			for (size_t i = 0; i < spelledTokens; ++i) {
				advance();
			}
		};
		const std::string_view text = resolved;

		Type type;
		const bool tagHidden = objectHidesTag(resolved);
		if (!tagHidden && resolveTypeName(text, type) && type.namedType.empty()) {
			// A type name in expression position constructs a value: float(x),
			// float3(0), float4(a, b, c, 1). T(...) is a constructor call, and the
			// parenthesised part is a list of arguments; (T)x is parseCast's.
			consumeName();

			if (type.isStdArray) {
				if (!at(TokenKind::LBrace)) {
					throw CompileError("array<T, N> in an expression is lowered as a braced list, " + std::string(text) + "{...}");
				}
				ExpressionPtr list = parseInitializerList();
				list->constructType = type;
				return list;
			}
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

		// A struct's name and a braced list is a value of that struct: S{a, b} and
		// S{.field = a}. It is spelled in a return, an argument or an initialiser.
		// A typedef of a struct names the same struct: T{a, b} after "typedef S T;".
		std::string braced = resolved;
		if (Type aliased; resolveTypeName(text, aliased) && !aliased.namedType.empty()
			&& _unit.findStruct(aliased.namedType)) {
			braced = aliased.namedType;
		}
		if (!tagHidden && (fileScoped || !isLocal(resolved)) && _unit.findStruct(braced) && lookahead(spelledTokens).kind == TokenKind::LBrace) {
			consumeName();
			ExpressionPtr list = parseInitializerList();
			Type type;
			type.namedType = braced;
			list->constructType = type;
			list->namesConcreteStruct = resolved.rfind("<template ", 0) != 0;
			return list;
		}

		// A struct's name and an argument list call its constructor.
		if (!tagHidden && (fileScoped || !isLocal(resolved)) && _constructors.count(braced) && lookahead(spelledTokens).kind == TokenKind::LParen) {
			const size_t atLine = line();
			consumeName();
			advance();
			Type type;
			type.namedType = braced;
			std::vector<ExpressionPtr> arguments = parseArgumentList("to close a constructor's argument list");
			ExpressionPtr call = constructWithConstructor(type, arguments, atLine);
			if (call) {
				call->namesConcreteStruct = resolved.rfind("<template ", 0) != 0;
			}
			if (!call) {
				throw CompileError("\"" + braced + "\" has a constructor with nothing to do, and a value of it is not lowered");
			}
			return call;
		}

		// INFINITY, HUGE_VALF, HUGE_VALH and NAN expand to these compiler builtins: each is
		// the one value its name says, so it is the float constant.
		if ((resolved == "__builtin_inff" || resolved == "__builtin_huge_valf" || resolved == "__builtin_huge_valh"
			|| resolved == "__builtin_nanf") && lookahead().kind == TokenKind::LParen) {
			const bool wantsString = resolved == "__builtin_nanf";
			if (wantsString ? !(lookahead(2).kind == TokenKind::StringLiteral && lookahead(3).kind == TokenKind::RParen)
				: lookahead(2).kind != TokenKind::RParen) {
				throw CompileError("\"" + resolved + "\" takes " + (wantsString ? "a string" : "no arguments"));
			}
			auto expression = std::make_unique<Expression>();
			expression->line = line();
			expression->kind = ExpressionKind::FloatLiteral;
			expression->floatValue = wantsString ? std::numeric_limits<double>::quiet_NaN()
				: std::numeric_limits<double>::infinity();
			expression->floatIsHalf = resolved == "__builtin_huge_valh";
			consumeName();
			advance();
			if (wantsString) {
				advance();
			}
			advance();
			return expression;
		}

		// An enumerator is the integer it was declared with.
		const auto constant = _enumConstants.find(resolved);
		if (constant != _enumConstants.end()) {
			auto expression = std::make_unique<Expression>();
			expression->line = line();
			consumeName();
			expression->kind = ExpressionKind::IntLiteral;
			const int64_t value = constant->second.value;
			if (constant->second.kind == ScalarKind::Bool) {
				// A bool template parameter's value.
				expression->kind = ExpressionKind::BoolLiteral;
				expression->boolValue = value != 0;
				return expression;
			}
			expression->intKind = constant->second.kind;
			expression->enumName = constant->second.owner;
			expression->intValue = static_cast<uint64_t>(value < 0 ? -value : value);
			if (value >= 0) {
				return expression;
			}

			auto negated = std::make_unique<Expression>();
			negated->kind = ExpressionKind::Unary;
			negated->line = expression->line;
			negated->unaryOperator = UnaryOperator::Negate;
			negated->enumName = expression->enumName;
			negated->left = std::move(expression);
			measure(*negated);
			return negated;
		}

		// "f<float, 3>(x)": the "<" after a function template's name opens its arguments. A name that
		// a block declares (a local, a using declaration) is not looked up as the template.
		const bool blocked = spelledTokens == 1 && blockName(std::string(current().text));
		const auto matchesSet = [&](const std::set<std::string>& set) {
			return set.count(resolved) || std::any_of(alternatives.begin(), alternatives.end(),
				[&](const std::string& each) { return set.count(each) != 0; });
		};
		const bool templateId = lookahead(spelledTokens).kind == TokenKind::Less && !blocked && matchesSet(_unit.functionTemplates);
		if (!templateId && lookahead(spelledTokens).kind == TokenKind::Less && !blocked && matchesSet(_entryTemplateNames)) {
			throw CompileError("cannot call the entry point \"" + resolved + "\"");
		}

		// A name that is also an MSL builtin is a plain identifier here. Whether
		// it is the builtin or a parameter that shadows one is a question about
		// scope, which the parser has no answer for, so the emitter decides.
		auto expression = std::make_unique<Expression>();
		expression->kind = ExpressionKind::Identifier;
		expression->line = line();
		expression->name = resolved;
		expression->overloadNames = alternatives;
		expression->templateDependent = spelledTokens == 1 && namesDependentParameter(std::string(current().text));
		// An unqualified name in a call also finds the functions of its arguments' namespaces.
		expression->unqualifiedCall = spelledTokens == 1 && lookahead(spelledTokens).kind == TokenKind::LParen
			&& !isLocal(std::string(current().text));
		expression->fileScope = fileScoped;
		// A qualified call is looked up at the scope it WROTE, not at the scope the member
		// it found happens to live in: "B::clamp" can resolve to "A::clamp" through a using
		// declaration or directive in B, but it is still B's lookup that also reaches the
		// library when B brings metal in too (directly, as namespace metal itself, or
		// transitively), so collectMembers walks from the written scope.
		if (spelled.parts.size() > 1) {
			const std::string scope = namespaceOf(spelled, spelled.parts.size() - 1);
			if (!scope.empty()) {
				std::set<std::string> seen, found;
				collectMembers(scope, spelled.parts.back(), seen, found, true, &expression->stdlibBeside);
			}
		} else if (spelled.global) {
			collectStdlibOwn("", spelled.parts.back(), nullptr, &expression->stdlibBeside);
		}
		if (spelledTokens == 1 && kind() == TokenKind::Identifier && !blockName(std::string(current().text))) {
			unqualifiedFound(std::string(current().text), &expression->stdlibBeside);
		}
		consumeName();
		if (templateId) {
			advance();
			expression->hasTemplateArguments = true;
			while (!at(TokenKind::Greater)) {
				expression->templateArguments.push_back(parseTemplateArgument());
				if (!match(TokenKind::Comma)) {
					break;
				}
			}
			expect(TokenKind::Greater, "to close the template arguments");
			expression->unqualifiedCall = spelledTokens == 1 && at(TokenKind::LParen) && !isLocal(std::string(spelled.parts.back()));
		}
		return expression;
	}

	throw CompileError("unexpected " + std::string(tokenKindName(kind())) + " \""
		+ std::string(current().text) + "\" in an expression");
}

}
