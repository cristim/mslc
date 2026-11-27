#include "preprocessor/internal.h"

#include "predefined_macros.inc"

namespace mslc {

namespace pp {

namespace {

	// The simd headers as the Metal toolchain ships them: each has its own guard
	// macro, and <simd/simd.h> includes the other three. Only the matrix typedefs
	// are declarations; the vector typedefs (vector_float4, simd_uint2) are not here
	// because Apple's compiler has them without any include, so the parser has them
	// too. They are written as the source they stand for, so the parser reads the
	// typedefs like any other and the preprocessor needs no second path for them.
	constexpr char kSimdMatrixTypesHeader[] = R"SIMD(#ifndef __SIMD_MATRIX_TYPES_HEADER__
#define __SIMD_MATRIX_TYPES_HEADER__
#include <metal_matrix>
typedef half2x2 matrix_half2x2;
typedef half3x2 matrix_half3x2;
typedef half4x2 matrix_half4x2;
typedef half2x3 matrix_half2x3;
typedef half3x3 matrix_half3x3;
typedef half4x3 matrix_half4x3;
typedef half2x4 matrix_half2x4;
typedef half3x4 matrix_half3x4;
typedef half4x4 matrix_half4x4;
typedef float2x2 matrix_float2x2;
typedef float3x2 matrix_float3x2;
typedef float4x2 matrix_float4x2;
typedef float2x3 matrix_float2x3;
typedef float3x3 matrix_float3x3;
typedef float4x3 matrix_float4x3;
typedef float2x4 matrix_float2x4;
typedef float3x4 matrix_float3x4;
typedef float4x4 matrix_float4x4;
typedef half2x2 simd_half2x2;
typedef half3x2 simd_half3x2;
typedef half4x2 simd_half4x2;
typedef half2x3 simd_half2x3;
typedef half3x3 simd_half3x3;
typedef half4x3 simd_half4x3;
typedef half2x4 simd_half2x4;
typedef half3x4 simd_half3x4;
typedef half4x4 simd_half4x4;
typedef float2x2 simd_float2x2;
typedef float3x2 simd_float3x2;
typedef float4x2 simd_float4x2;
typedef float2x3 simd_float2x3;
typedef float3x3 simd_float3x3;
typedef float4x3 simd_float4x3;
typedef float2x4 simd_float2x4;
typedef float3x4 simd_float3x4;
typedef float4x4 simd_float4x4;
#endif
)SIMD";

	constexpr char kSimdPackedHeader[] = R"SIMD(#ifndef __SIMD_PACKED_HEADER__
#define __SIMD_PACKED_HEADER__
#endif
)SIMD";

	constexpr char kSimdVectorTypesHeader[] = R"SIMD(#ifndef __SIMD_VECTOR_TYPES_HEADER__
#define __SIMD_VECTOR_TYPES_HEADER__
#endif
)SIMD";

	constexpr char kSimdHeader[] = R"SIMD(#ifndef __SIMD_HEADER__
#define __SIMD_HEADER__
#include <simd/matrix_types.h>
#include <simd/packed.h>
#include <simd/vector_types.h>
#endif
)SIMD";

	// The <metal_stdlib> sub-headers whose declarations <metal_stdlib> already has built in.
	// Each reads as <metal_stdlib> itself, so the two share one include-once state.
	// Apple has the rest of the metal_* family; mslc does not declare what they hold.
	constexpr const char* kStdlibSubHeaders[] = {"<metal_texture>", "<metal_math>", "<metal_common>",
		"<metal_geometric>", "<metal_integer>", "<metal_relational>", "<metal_graphics>", "<metal_types>", "<metal_atomic>", "<metal_pack>"};

}

// The built-in header a name stands for: a stdlib sub-header is <metal_stdlib>.
const std::string& builtinHeaderKey(const std::string& name) {
	static const std::string stdlib = "<metal_stdlib>";
	for (const char* sub : kStdlibSubHeaders) {
		if (name == sub) { return stdlib; }
	}
	return name;
}

// The text of a header mslc has built in, or null for any other name.
const char* builtinHeaderText(const std::string& rawName) {
	const std::string& name = builtinHeaderKey(rawName);
	if (name == "<metal_stdlib>") { return kMetalStdlibMacros; }
	if (name == "<metal_matrix>") { return kMetalMatrixMacros; }
	if (name == "<simd/simd.h>") { return kSimdHeader; }
	if (name == "<simd/matrix_types.h>") { return kSimdMatrixTypesHeader; }
	if (name == "<simd/packed.h>") { return kSimdPackedHeader; }
	if (name == "<simd/vector_types.h>") { return kSimdVectorTypesHeader; }
	return nullptr;
}

void spliceLines(std::string_view raw, std::string& text, std::vector<size_t>& lineStarts) {
	text.reserve(raw.size());
	lineStarts.push_back(0);

	for (size_t i = 0; i < raw.size(); ++i) {
		const char c = raw[i];

		if (c == '\\') {
			// Apple's compiler also splices when blanks sit between the backslash
			// and the newline, which editors leave behind without anyone noticing.
			size_t next = i + 1;
			while (next < raw.size() && (raw[next] == ' ' || raw[next] == '\t' || raw[next] == '\f'
				|| raw[next] == '\v')) {
				++next;
			}
			if (next + 1 < raw.size() && raw[next] == '\r' && raw[next + 1] == '\n') {
				++next;
			}
			if (next < raw.size() && (raw[next] == '\n' || raw[next] == '\r')) {
				i = next;
				lineStarts.push_back(text.size());
				continue;
			}
		}

		// A carriage return alone ends a line, as it does for Apple's compiler;
		// before \n it is part of a CRLF and stays.
		const bool loneReturn = c == '\r' && !(i + 1 < raw.size() && raw[i + 1] == '\n');
		text.push_back(loneReturn ? '\n' : c);
		if (c == '\n' || loneReturn) {
			lineStarts.push_back(text.size());
		}
	}
}

bool isWithin(const fs::path& path, const fs::path& root) {
	auto rootIt = root.begin();
	auto pathIt = path.begin();

	for (; rootIt != root.end(); ++rootIt, ++pathIt) {
		if (pathIt == path.end() || *pathIt != *rootIt) {
			return false;
		}
	}
	return true;
}

} // namespace pp

using namespace pp;

std::string Preprocessor::describe(const Loc& loc) const {
	if (!loc.file) {
		return "<unknown>";
	}
	return loc.file->displayPath + ":" + std::to_string(loc.line) + ":" + std::to_string(loc.column);
}

void Preprocessor::fail(const Loc& loc, const std::string& message) const {
	std::string text = describe(loc) + ": " + message;

	// A cycle fails at the depth limit, and its chain would otherwise be that long.
	constexpr size_t kShownIncludes = 10;
	size_t shown = 0;
	for (auto it = _includes.rbegin(); it != _includes.rend(); ++it, ++shown) {
		if (shown == kShownIncludes) {
			text += "\n  ... and " + std::to_string(_includes.size() - kShownIncludes) + " more";
			break;
		}
		text += "\n  included from " + describe(*it);
	}
	throw CompileError(text);
}

size_t Preprocessor::internName(const std::string& name) {
	_names.push_back(name);
	return _names.size() - 1;
}

SourceFile& Preprocessor::loadText(std::string displayPath, std::string canonical, std::string_view raw) {
	auto file = std::make_unique<SourceFile>();
	file->displayPath = std::move(displayPath);
	file->canonical = std::move(canonical);
	spliceLines(raw, file->text, file->lineStarts);

	try {
		file->tokens = tokenize(file->text, true);
	} catch (const LexError& error) {
		Token at;
		at.offset = error.offset();
		Cursor cursor(*file);
		fail(locate(cursor, at), error.what());
	}

	SourceFile& ref = *file;
	_owned.push_back(std::move(file));
	if (!ref.canonical.empty()) {
		_files[ref.canonical] = &ref;
	}
	return ref;
}

Loc Preprocessor::locate(const Cursor& cursor, const Token& token) const {
	const auto& starts = cursor.file.lineStarts;
	const size_t line = static_cast<size_t>(std::upper_bound(starts.begin(), starts.end(), token.offset)
		- starts.begin());

	Loc loc;
	loc.file = &cursor.file;
	loc.line = line;
	loc.column = token.offset - starts[line - 1] + 1;
	loc.presumedLine = static_cast<size_t>(static_cast<long>(line) + cursor.lineDelta);
	loc.nameId = cursor.nameId;
	return loc;
}

PPTok Preprocessor::fromFile(const Cursor& cursor, const Token& token) const {
	PPTok result;
	result.tok = token;
	result.loc = locate(cursor, token);
	return result;
}

TokenOrigin Preprocessor::originOf(const Loc& loc) {
	auto found = _fileIndex.find(loc.file);
	if (found == _fileIndex.end()) {
		found = _fileIndex.emplace(loc.file, static_cast<uint32_t>(_fileNames.size())).first;
		_fileNames.push_back(loc.file->displayPath);
	}

	TokenOrigin origin;
	origin.file = found->second;
	origin.line = static_cast<uint32_t>(loc.line);
	origin.column = static_cast<uint32_t>(loc.column);
	return origin;
}

void Preprocessor::flush(std::vector<PPTok>& out) {
	if (_output.size() + out.size() > kMaxOutputTokens) {
		throw CompileError("the preprocessed source is larger than " + std::to_string(kMaxOutputTokens)
			+ " tokens");
	}

	for (PPTok& pp: out) {
		if (pp.tok.kind == TokenKind::Invalid) {
			fail(pp.loc, describeInvalidToken(pp.tok));
		}

		pp.tok.line = pp.loc.line;
		_output.push_back(pp.tok);
		_origins.push_back(originOf(pp.loc));
	}
	out.clear();
}

SourceFile* Preprocessor::loadFile(const fs::path& path, const std::string& displayPath, const Loc& at,
	const std::string& directive) {
	std::error_code ec;
	const std::string canonical = fs::canonical(path, ec).string();
	if (ec) {
		fail(at, "cannot resolve " + directive + " \"" + displayPath + "\": " + ec.message());
	}

	const auto cached = _files.find(canonical);
	if (cached != _files.end()) {
		return cached->second;
	}

	if (!fs::is_regular_file(canonical, ec)) {
		fail(at, directive + " \"" + displayPath + "\" is not a regular file");
	}
	const auto size = fs::file_size(canonical, ec);
	if (ec || size > kMaxFileBytes) {
		fail(at, directive + " \"" + displayPath + "\" is larger than " + std::to_string(kMaxFileBytes)
			+ " bytes or cannot be sized");
	}

	std::ifstream stream(canonical, std::ios::binary);
	std::string raw((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (!stream && !stream.eof()) {
		fail(at, "cannot read " + directive + " \"" + displayPath + "\"");
	}

	return &loadText(displayPath, canonical, raw);
}

// Resolves a quoted include against the including file's directory and then the
// include directories. Returns null when no candidate exists. A name that is
// absolute, or that reaches outside the allowed roots by "..", a symlink or
// otherwise, is an error: it is the boundary between the shader and the rest of
// the filesystem.
SourceFile* Preprocessor::resolveQuoted(const SourceFile& includer, const std::string& name, const Loc& at,
	const std::string& directive) {
	if (name.empty()) {
		fail(at, directive + " with an empty file name");
	}
	if (name.find('\0') != std::string::npos) {
		fail(at, directive + " file name contains a NUL byte");
	}
	if (name[0] == '/') {
		fail(at, directive + " \"" + name + "\" is an absolute path, which is not allowed; use a path "
			"relative to the including file or to an include directory");
	}

	struct Base {
		fs::path directory;
		std::string display;
	};
	std::vector<Base> bases;

	if (!includer.canonical.empty()) {
		bases.push_back({ fs::path(includer.canonical).parent_path(),
			fs::path(includer.displayPath).parent_path().string() });
	}
	for (size_t i = 0; i < _options.includeDirs.size(); ++i) {
		bases.push_back({ _roots[_roots.size() - _options.includeDirs.size() + i], _options.includeDirs[i] });
	}

	bool escaped = false;
	for (const Base& base: bases) {
		// The bounds are checked on the normalised path first, so a name that leaves
		// the roots is refused without asking the filesystem about it. The
		// filesystem is then asked about the path as written, because "x/../a.h" is
		// not "a.h" unless x exists.
		const fs::path joined = base.directory / name;
		const fs::path candidate = joined.lexically_normal();
		const bool lexicallyInside = std::any_of(_roots.begin(), _roots.end(),
			[&](const fs::path& root) { return isWithin(candidate, root); });
		if (!lexicallyInside) {
			escaped = true;
			continue;
		}

		std::error_code ec;
		if (!fs::exists(joined, ec)) {
			continue;
		}

		const fs::path real = fs::canonical(joined, ec);
		const bool realInside = !ec && std::any_of(_roots.begin(), _roots.end(),
			[&](const fs::path& root) { return isWithin(real, root); });
		if (!realInside) {
			escaped = true;
			continue;
		}

		const std::string display = (fs::path(base.display.empty() ? "." : base.display) / name).string();
		return loadFile(real, display, at, directive);
	}

	if (escaped) {
		fail(at, directive + " \"" + name + "\" resolves outside the directories it may include from "
			"(the including file's and the include directories)");
	}
	return nullptr;
}

Preprocessor::Header Preprocessor::parseHeaderName(const std::vector<Token>& line, size_t first,
	const Loc& at, const std::string& directive) const {

	if (first >= line.size()) {
		fail(at, directive + " with no file name");
	}

	Header header;
	if (isStringLiteral(line[first])) {
		header.angled = false;
		header.name = std::string(line[first].text.substr(1, line[first].text.size() - 2));
		if (first + 1 < line.size()) {
			fail(at, "extra tokens at the end of the " + directive);
		}
		return header;
	}

	if (line[first].kind != TokenKind::Less) {
		fail(at, directive + " expects \"file\" or <file>; a header named by a macro is not supported");
	}

	// Closed by the first '>' on the line. A name with a gap in it is a
	// different name: concatenating the tokens would turn "<metal_std lib>" into
	// the one header mslc honours.
	size_t last = first;
	for (size_t i = first + 1; i < line.size(); ++i) {
		if (line[i].kind == TokenKind::Greater) {
			last = i;
			break;
		}
	}
	if (last == first) {
		fail(at, "the " + directive + " has no '>' on its line, so the header name does not end here");
	}
	if (last + 1 < line.size()) {
		fail(at, "extra tokens at the end of the " + directive);
	}

	for (size_t i = first + 1; i <= last; ++i) {
		if (line[i].spaceBefore) {
			fail(at, "the " + directive + " has whitespace in the header name, which is part of the name it "
				"looks for");
		}
	}

	header.angled = true;
	header.name = "<";
	for (size_t i = first + 1; i <= last; ++i) {
		header.name += line[i].text;
	}
	return header;
}

// Whether a directive reads the file now. #pragma once and #import both stop every
// later #include or #import of the file; #import also yields to an earlier #include
// of it, which is what Apple's compiler does.
bool Preprocessor::admit(SourceFile& file, bool import) {
	// An #import marks the file whether or not it reads it: Clang decides to skip
	// after marking, so a later #include of the file is skipped too.
	const bool skip = file.once || (import && file.included);
	file.once = file.once || import;
	if (skip) {
		return false;
	}
	file.included = true;
	return true;
}

void Preprocessor::includeDirective(Cursor& cursor, const std::vector<Token>& line, const Loc& at,
	bool import) {

	const std::string directive = import ? "#import" : "#include";
	const Header header = parseHeaderName(line, 1, at, directive);

	if (header.angled) {
		// The headers whose contents mslc has built in. Any other angled name
		// would have to come from a system header search mslc does not have.
		const char* text = builtinHeaderText(header.name);
		if (!text) {
			fail(at, "cannot honour " + directive + " " + header.name + ": mslc provides " + kBuiltinHeaderList
				+ " only, and a header it does not read would leave the program compiling up to the "
				"first name it needed");
		}

		// The header's declarations are built in, but its macros are real, and a
		// shader may test them. <simd/simd.h> also carries its typedefs, which the
		// parser reads like any other source.
		SourceFile*& builtin = _builtinHeaders[builtinHeaderKey(header.name)];
		if (!builtin) {
			builtin = &loadText("<built-in>", "", text);
		}
		if (!admit(*builtin, import)) {
			return;
		}
		processFile(*builtin, internName(builtin->displayPath));
		return;
	}

	if (_includes.size() >= kMaxIncludeDepth) {
		fail(at, directive + " nested more than " + std::to_string(kMaxIncludeDepth) + " deep; is there "
			"an include cycle with no guard?");
	}
	if (++_includeCount > kMaxIncludes) {
		fail(at, "more than " + std::to_string(kMaxIncludes) + " #include directives in one translation");
	}

	SourceFile* target = resolveQuoted(cursor.file, header.name, at, directive);
	if (!target) {
		fail(at, directive + " \"" + header.name + "\" not found");
	}
	if (!admit(*target, import)) {
		return;
	}

	_includes.push_back(at);
	processFile(*target, internName(target->displayPath));
	_includes.pop_back();
}

void Preprocessor::processFile(SourceFile& file, size_t nameId) {
	Cursor cursor(file);
	cursor.nameId = nameId;

	std::deque<PPTok> work;
	std::vector<PPTok> out;
	scan(&cursor, work, out, 0);
	flush(out);

	if (!cursor.conditionals.empty()) {
		fail(cursor.conditionals.back().opener, "unterminated conditional directive");
	}
}

void Preprocessor::run(std::string_view source) {
	// The directories a quoted include may resolve into: the source's own, then
	// each include directory, in the order they are searched.
	std::error_code ec;
	std::string sourceCanonical;

	if (!_options.sourcePath.empty()) {
		const fs::path absolute = fs::absolute(_options.sourcePath, ec);
		const fs::path real = fs::weakly_canonical(absolute, ec);
		if (ec) {
			throw CompileError("cannot resolve the source path \"" + _options.sourcePath + "\": " + ec.message());
		}
		sourceCanonical = real.string();
		_roots.push_back(real.parent_path());
	}
	for (const std::string& directory: _options.includeDirs) {
		if (directory.empty()) {
			throw CompileError("an include directory is the empty string");
		}
		const fs::path real = fs::canonical(directory, ec);
		if (ec || !fs::is_directory(real, ec)) {
			throw CompileError("include directory \"" + directory + "\" does not exist or is not a directory");
		}
		_roots.push_back(real);
	}

	// Built-in macros, defined by running their #define lines through the same
	// code a source's own would go through.
	{
		SourceFile& predefined = loadText("<built-in>", "",
			std::string(kPredefinedMacros) + kImplicitImportMacros);
		processFile(predefined, internName("<built-in>"));
		_output.clear();
	}

	// Macros from the caller, defined by the same code a #define in the source goes through, so a name
	// or a value that is not valid fails the way the same text in the source would.
	if (!_options.macros.empty()) {
		std::string lines;
		for (const auto& [name, value]: _options.macros) {
			if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')
				|| std::any_of(name.begin(), name.end(), [](char c) {
					return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_');
				})) {
				throw CompileError("macro name \"" + name + "\" is not an identifier");
			}
			if (value.find_first_of("\r\n") != std::string::npos) {
				throw CompileError("the value of macro \"" + name + "\" contains a line break");
			}
			lines += "#define " + name + (value.empty() ? "" : " ") + value + "\n";
		}
		SourceFile& commandLine = loadText("<command line>", "", lines);
		_definingPredefined = true;
		processFile(commandLine, internName("<command line>"));
		_definingPredefined = false;
		_output.clear();
	}

	_basePath = _options.sourcePath.empty() ? "<source>" : _options.sourcePath;
	SourceFile& root = loadText(_basePath, sourceCanonical, source);
	root.included = true;
	processFile(root, internName(_basePath));

	Token end;
	end.kind = TokenKind::EndOfFile;
	end.offset = 0;
	end.line = _output.empty() ? 1 : _output.back().line;
	end.text = std::string_view();
	_output.push_back(end);
	_origins.push_back(_origins.empty() ? TokenOrigin() : _origins.back());
}

PreprocessedSource preprocess(std::string_view source, const PreprocessOptions& options) {
	auto state = std::make_shared<Preprocessor>(options);
	state->run(source);

	PreprocessedSource result;
	result.tokens = state->takeOutput();
	result.origins = state->takeOrigins();
	result.fileNames = state->takeFileNames();
	result.storage = state;
	return result;
}

std::string describeOrigin(const PreprocessedSource& source, size_t tokenIndex) {
	if (tokenIndex >= source.origins.size()) {
		return "<unknown>";
	}

	const TokenOrigin& origin = source.origins[tokenIndex];
	if (origin.file >= source.fileNames.size()) {
		return "<unknown>";
	}
	return source.fileNames[origin.file] + ":" + std::to_string(origin.line) + ":" + std::to_string(origin.column);
}

std::string renderTokens(const std::vector<Token>& tokens) {
	std::string text;
	const Token* previous = nullptr;

	for (const Token& token: tokens) {
		if (token.kind == TokenKind::EndOfFile) {
			break;
		}

		if (previous) {
			if (token.startOfLine) {
				text += '\n';
			} else {
				bool space = token.spaceBefore;
				if (!space) {
					// Tokens that were apart in the source can end up adjacent after
					// expansion, and the text would then read as one token.
					const std::string joined = std::string(previous->text) + std::string(token.text);
					const std::vector<Token> relexed = tokenize(joined, true);
					space = relexed.size() != 3 || relexed[0].text != previous->text;
				}
				if (space) {
					text += ' ';
				}
			}
		}

		text += token.text;
		previous = &token;
	}

	text += '\n';
	return text;
}

} // namespace mslc
