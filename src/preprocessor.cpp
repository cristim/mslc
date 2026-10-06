#include "preprocessor.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>

#include "predefined_macros.inc"

namespace mslc {

namespace fs = std::filesystem;

namespace {

	// The limits exist because the input is untrusted: an include that includes
	// itself twice, or a macro that doubles on every expansion, would otherwise
	// run until memory is gone. Each is far past what a real shader reaches.
	constexpr size_t kMaxIncludeDepth = 200;
	constexpr size_t kMaxIncludes = 10000;
	constexpr size_t kMaxOutputTokens = size_t(1) << 20;
	constexpr size_t kMaxExpandedTokens = size_t(1) << 24;
	constexpr size_t kMaxPendingTokens = size_t(1) << 18;
	constexpr size_t kMaxHeldArgumentTokens = size_t(1) << 19;
	constexpr size_t kMaxFileBytes = size_t(32) << 20;
	constexpr size_t kMaxExpansionDepth = 200;
	constexpr size_t kMaxConditionalDepth = 1000;
	constexpr size_t kMaxExpressionDepth = 200;

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

	constexpr char kBuiltinHeaderList[] = "<metal_stdlib>, <metal_matrix>, <simd/simd.h>, <simd/matrix_types.h>, "
		"<simd/packed.h>, <simd/vector_types.h> and the <metal_stdlib> sub-headers <metal_texture>, "
		"<metal_math>, <metal_common>, <metal_geometric>, <metal_integer>, <metal_relational>, "
		"<metal_graphics> and <metal_types>";

	// The <metal_stdlib> sub-headers whose declarations <metal_stdlib> already has built in.
	// Each reads as <metal_stdlib> itself, so the two share one include-once state.
	// Apple has the rest of the metal_* family; mslc does not declare what they hold.
	constexpr const char* kStdlibSubHeaders[] = {"<metal_texture>", "<metal_math>", "<metal_common>",
		"<metal_geometric>", "<metal_integer>", "<metal_relational>", "<metal_graphics>", "<metal_types>"};

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

	struct SourceFile {
		std::string displayPath;
		std::string canonical;
		std::string text;
		std::vector<size_t> lineStarts;
		std::vector<Token> tokens;
		bool once = false;
		// Read at least once, by #include or #import.
		bool included = false;
	};

	struct Loc {
		const SourceFile* file = nullptr;
		size_t line = 0;
		size_t column = 0;
		size_t presumedLine = 0;
		size_t nameId = 0;
	};

	struct Macro;
	using HideSet = std::shared_ptr<const std::vector<const Macro*>>;

	bool hideSetHas(const HideSet& set, const Macro* macro) {
		return set && std::binary_search(set->begin(), set->end(), macro, std::less<const Macro*>());
	}

	HideSet hideSetAdd(const HideSet& set, const Macro* macro) {
		auto result = std::make_shared<std::vector<const Macro*>>();
		if (set) {
			*result = *set;
		}
		result->insert(std::upper_bound(result->begin(), result->end(), macro, std::less<const Macro*>()), macro);
		return result;
	}

	HideSet hideSetUnion(const HideSet& a, const HideSet& b) {
		if (!b || b->empty() || a == b) {
			return a;
		}
		if (!a || a->empty()) {
			return b;
		}

		auto result = std::make_shared<std::vector<const Macro*>>();
		std::set_union(a->begin(), a->end(), b->begin(), b->end(), std::back_inserter(*result),
			std::less<const Macro*>());
		return result;
	}

	HideSet hideSetIntersect(const HideSet& a, const HideSet& b) {
		if (!a || !b) {
			return nullptr;
		}
		auto result = std::make_shared<std::vector<const Macro*>>();
		std::set_intersection(a->begin(), a->end(), b->begin(), b->end(), std::back_inserter(*result),
			std::less<const Macro*>());
		return result;
	}

	struct PPTok {
		Token tok;
		HideSet hide;
		Loc loc;
		bool placemarker = false;
	};

	enum class BodyKind { Token, Param, Stringify, Paste };

	struct BodyItem {
		BodyKind kind = BodyKind::Token;
		Token tok;
		size_t param = 0;
		bool spaceBefore = false;
	};

	struct Macro {
		std::string name;
		bool functionLike = false;
		bool variadic = false;
		std::string variadicName = "__VA_ARGS__";
		std::vector<std::string> params;
		std::vector<BodyItem> body;
		Loc definedAt;
	};

	enum class Builtin { Line, File, BaseFile, IncludeLevel, Counter };

	std::optional<Builtin> builtinByName(std::string_view name) {
		if (name == "__LINE__") return Builtin::Line;
		if (name == "__FILE__") return Builtin::File;
		if (name == "__BASE_FILE__") return Builtin::BaseFile;
		if (name == "__INCLUDE_LEVEL__") return Builtin::IncludeLevel;
		if (name == "__COUNTER__") return Builtin::Counter;
		return std::nullopt;
	}

	// Builtins of Apple's compiler that mslc does not provide. They are named
	// rather than left to read as undefined, which in #if would be 0.
	bool isUnsupportedBuiltin(std::string_view name) {
		static const char* const names[] = {
			"__DATE__", "__TIME__", "__TIMESTAMP__", "__FILE_NAME__", "__has_include_next",
			"__has_feature", "__has_extension", "__has_builtin", "__has_constexpr_builtin",
			"__has_attribute", "__has_cpp_attribute", "__has_c_attribute",
			"__has_declspec_attribute", "__has_warning", "__has_embed", "__is_identifier",
			"__is_target_arch", "__is_target_vendor", "__is_target_os",
			"__is_target_environment", "__is_target_variant_os",
			"__is_target_variant_environment", "__building_module", "_Pragma", "__VA_OPT__",
		};

		for (const char* candidate: names) {
			if (name == candidate) {
				return true;
			}
		}
		return false;
	}

	// Names a #define or #undef may not touch, because they are computed or are
	// operators rather than stored text.
	bool isBuiltinName(std::string_view name) {
		return name == "defined" || name == "__VA_ARGS__" || name == "__has_include"
			|| builtinByName(name) || isUnsupportedBuiltin(name);
	}

	// C++'s spellings of operators, which Apple's compiler refuses as macro names.
	bool isAlternativeOperator(std::string_view name) {
		static const char* const names[] = {
			"and", "or", "not", "xor", "bitand", "bitor", "compl", "and_eq", "or_eq", "xor_eq", "not_eq",
		};

		for (const char* candidate: names) {
			if (name == candidate) {
				return true;
			}
		}
		return false;
	}

	bool isKeyword(std::string_view name) {
		static const char* const keywords[] = {
			"alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch", "char",
			"class", "const", "const_cast", "constexpr", "continue", "decltype", "default",
			"delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export",
			"extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
			"mutable", "namespace", "new", "noexcept", "nullptr", "operator", "private",
			"protected", "public", "register", "reinterpret_cast", "return", "short", "signed",
			"sizeof", "static", "static_assert", "static_cast", "struct", "switch", "template",
			"this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
			"union", "unsigned", "using", "virtual", "void", "volatile", "while",
			// MSL's own qualifiers and entry-point keywords
			"constant", "device", "fragment", "kernel", "thread", "threadgroup", "vertex",
		};

		for (const char* candidate: keywords) {
			if (name == candidate) {
				return true;
			}
		}
		return false;
	}

	// The lexer reports a string literal as an Identifier token, so a token of
	// that kind is a name only if it starts like one.
	bool isName(const Token& token) {
		return token.kind == TokenKind::Identifier && !token.text.empty()
			&& (std::isalpha(static_cast<unsigned char>(token.text[0])) || token.text[0] == '_');
	}

	bool isStringLiteral(const Token& token) {
		return token.kind == TokenKind::Identifier && !token.text.empty() && token.text[0] == '"';
	}

	// "1L", "0b101" and "1e+5" are each one preprocessing number in C, though the
	// lexer reads them as several tokens, so a paste that forms one is valid.
	bool isPreprocessingNumber(std::string_view text) {
		size_t i = 0;
		if (i < text.size() && text[i] == '.') {
			++i;
		}
		if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) {
			return false;
		}

		for (++i; i < text.size(); ++i) {
			const char c = text[i];
			if ((c == '+' || c == '-') && std::string_view("eEpP").find(text[i - 1]) != std::string_view::npos) {
				continue;
			}
			if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.')) {
				return false;
			}
		}
		return true;
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

	enum class Directive {
		Define, Undef, Include, Import, If, Ifdef, Ifndef, Elif, Else, Endif, Line, Error, Warning,
		Pragma, Unsupported, Unknown,
	};

	Directive directiveByName(std::string_view name) {
		static const std::map<std::string, Directive, std::less<>> table = {
			{ "define", Directive::Define }, { "undef", Directive::Undef },
			{ "include", Directive::Include }, { "if", Directive::If },
			{ "ifdef", Directive::Ifdef }, { "ifndef", Directive::Ifndef },
			{ "elif", Directive::Elif }, { "else", Directive::Else },
			{ "endif", Directive::Endif }, { "line", Directive::Line },
			{ "error", Directive::Error }, { "warning", Directive::Warning },
			{ "pragma", Directive::Pragma },
			{ "import", Directive::Import }, { "include_next", Directive::Unsupported },
			{ "ident", Directive::Unsupported }, { "sccs", Directive::Unsupported },
			{ "assert", Directive::Unsupported }, { "unassert", Directive::Unsupported },
		};

		const auto it = table.find(name);
		return it == table.end() ? Directive::Unknown : it->second;
	}

	// Apple's front end reports __INTMAX_WIDTH__ as 128, and #if arithmetic is done
	// in intmax_t, so 64 bits would disagree with it on every value past 2^63.
	using Wide = unsigned __int128;
	using SignedWide = __int128;
	constexpr Wide kWideMax = ~Wide(0);
	constexpr Wide kSignBit = Wide(1) << 127;
	constexpr unsigned kWideBits = 128;

	struct Value {
		Wide bits = 0;
		bool isUnsigned = false;

		SignedWide asSigned() const { return static_cast<SignedWide>(bits); }
		bool truthy() const { return bits != 0; }
	};

	enum class BinaryOp {
		None, Mul, Div, Mod, Add, Sub, Shl, Shr, Lt, Le, Gt, Ge, Eq, Ne, BitAnd, BitXor,
		BitOr, LogicalAnd, LogicalOr,
	};

	int precedence(BinaryOp op) {
		switch (op) {
			case BinaryOp::Mul: case BinaryOp::Div: case BinaryOp::Mod: return 10;
			case BinaryOp::Add: case BinaryOp::Sub: return 9;
			case BinaryOp::Shl: case BinaryOp::Shr: return 8;
			case BinaryOp::Lt: case BinaryOp::Le: case BinaryOp::Gt: case BinaryOp::Ge: return 7;
			case BinaryOp::Eq: case BinaryOp::Ne: return 6;
			case BinaryOp::BitAnd: return 5;
			case BinaryOp::BitXor: return 4;
			case BinaryOp::BitOr: return 3;
			case BinaryOp::LogicalAnd: return 2;
			case BinaryOp::LogicalOr: return 1;
			case BinaryOp::None: break;
		}
		return 0;
	}

	BinaryOp binaryOpFor(const Token& token) {
		switch (token.kind) {
			case TokenKind::Star: return BinaryOp::Mul;
			case TokenKind::Slash: return BinaryOp::Div;
			case TokenKind::Percent: return BinaryOp::Mod;
			case TokenKind::Plus: return BinaryOp::Add;
			case TokenKind::Minus: return BinaryOp::Sub;
			case TokenKind::ShiftLeft: return BinaryOp::Shl;
			case TokenKind::ShiftRight: return BinaryOp::Shr;
			case TokenKind::Less: return BinaryOp::Lt;
			case TokenKind::LessEqual: return BinaryOp::Le;
			case TokenKind::Greater: return BinaryOp::Gt;
			case TokenKind::GreaterEqual: return BinaryOp::Ge;
			case TokenKind::Equal: return BinaryOp::Eq;
			case TokenKind::NotEqual: return BinaryOp::Ne;
			case TokenKind::Ampersand: return BinaryOp::BitAnd;
			case TokenKind::Caret: return BinaryOp::BitXor;
			case TokenKind::Pipe: return BinaryOp::BitOr;
			case TokenKind::AndAnd: return BinaryOp::LogicalAnd;
			case TokenKind::OrOr: return BinaryOp::LogicalOr;
			case TokenKind::Identifier:
				if (token.text == "and") return BinaryOp::LogicalAnd;
				if (token.text == "or") return BinaryOp::LogicalOr;
				if (token.text == "bitand") return BinaryOp::BitAnd;
				if (token.text == "xor") return BinaryOp::BitXor;
				if (token.text == "bitor") return BinaryOp::BitOr;
				if (token.text == "not_eq") return BinaryOp::Ne;
				return BinaryOp::None;
			default: return BinaryOp::None;
		}
	}

	struct Conditional {
		Loc opener;
		bool live = true;
		bool taken = false;
		bool seenElse = false;
	};

	struct Cursor {
		explicit Cursor(SourceFile& f): file(f) {}

		SourceFile& file;
		size_t index = 0;
		std::vector<Conditional> conditionals;
		long lineDelta = 0;
		size_t nameId = 0;
	};

	struct Expression {
		std::vector<PPTok> tokens;
		size_t position = 0;
		size_t depth = 0;
		Loc at;
	};

}

class Preprocessor {
public:
	explicit Preprocessor(const PreprocessOptions& options): _options(options) {}

	void run(std::string_view source);

	std::vector<Token> takeOutput() { return std::move(_output); }
	std::vector<TokenOrigin> takeOrigins() { return std::move(_origins); }
	std::vector<std::string> takeFileNames() { return std::move(_fileNames); }

private:
	PreprocessOptions _options;
	std::vector<Token> _output;
	std::vector<TokenOrigin> _origins;
	std::vector<std::string> _fileNames;
	std::map<const SourceFile*, uint32_t> _fileIndex;
	std::map<std::string, SourceFile*> _builtinHeaders;
	std::deque<std::string> _storage;
	std::vector<std::unique_ptr<SourceFile>> _owned;
	std::map<std::string, SourceFile*> _files;
	std::map<std::string, std::unique_ptr<Macro>, std::less<>> _macros;
	std::vector<std::string> _names;
	std::vector<fs::path> _roots;
	std::vector<Loc> _includes;
	std::string _basePath;
	size_t _counter = 0;
	size_t _includeCount = 0;
	size_t _expanded = 0;
	size_t _heldArguments = 0;

	[[noreturn]] void fail(const Loc& loc, const std::string& message) const;
	std::string describe(const Loc& loc) const;

	SourceFile& loadText(std::string displayPath, std::string canonical, std::string_view raw);
	size_t internName(const std::string& name);
	Loc locate(const Cursor& cursor, const Token& token) const;
	PPTok fromFile(const Cursor& cursor, const Token& token) const;

	void processFile(SourceFile& file, size_t nameId);
	void scan(Cursor* cursor, std::deque<PPTok>& work, std::vector<PPTok>& out, size_t depth);
	void flush(std::vector<PPTok>& out);
	TokenOrigin originOf(const Loc& loc);

	// Directives
	void directive(Cursor& cursor);
	void defineMacro(Cursor& cursor, const std::vector<Token>& line, const Loc& at);
	void undefMacro(Cursor& cursor, const std::vector<Token>& line, const Loc& at);
	bool admit(SourceFile& file, bool import);
	void includeDirective(Cursor& cursor, const std::vector<Token>& line, const Loc& at, bool import);
	void lineDirective(Cursor& cursor, const std::vector<Token>& line, const Loc& at);
	void pragmaDirective(Cursor& cursor, const std::vector<Token>& line, const Loc& at);
	bool evaluateCondition(Cursor& cursor, const std::vector<Token>& line, const Loc& at);
	void requireNoExtra(const std::vector<Token>& line, size_t used, const char* directive,
		const Cursor& cursor) const;

	// Includes
	struct Header {
		bool angled = false;
		std::string name;
	};
	Header parseHeaderName(const std::vector<Token>& line, size_t first, const Loc& at,
		const std::string& directive) const;
	SourceFile* resolveQuoted(const SourceFile& includer, const std::string& name, const Loc& at,
		const std::string& directive);
	SourceFile* loadFile(const fs::path& path, const std::string& displayPath, const Loc& at,
		const std::string& directive);

	// Macro expansion
	bool isDefinedName(std::string_view name, const Loc& at) const;
	PPTok synthesize(const std::string& text, const PPTok& like);
	std::vector<Token> lexSynthetic(const std::string& text);
	PPTok pasteTokens(const PPTok& left, const PPTok& right, const Loc& at);
	PPTok stringify(const std::vector<PPTok>& argument, const PPTok& like, const Loc& at);
	std::vector<PPTok> substitute(const Macro& macro, const PPTok& name,
		const std::vector<std::vector<PPTok>>& arguments, bool variadicOmitted, const Loc& bodyLoc,
		size_t depth);
	std::vector<PPTok> expandBuiltin(Builtin builtin, const PPTok& name);

	// #if expressions
	Value parseTernary(Expression& expr, bool live);
	Value parseBinary(Expression& expr, int minPrecedence, bool live);
	Value parseUnary(Expression& expr, bool live);
	Value parseIntegerLiteral(Expression& expr);
	Value applyBinary(BinaryOp op, Value a, Value b, bool live, const Loc& at) const;
};

namespace {
	// The tokens' text, with a space where the source had whitespace.
	std::string spell(const std::vector<Token>& tokens, size_t first = 0) {
		std::string text;
		for (size_t i = first; i < tokens.size(); ++i) {
			if (i > first && tokens[i].spaceBefore) {
				text += ' ';
			}
			text += tokens[i].text;
		}
		return text;
	}
}

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

std::vector<Token> Preprocessor::lexSynthetic(const std::string& text) {
	_storage.push_back(text);
	std::vector<Token> tokens = tokenize(_storage.back(), true);
	tokens.pop_back();
	return tokens;
}

PPTok Preprocessor::synthesize(const std::string& text, const PPTok& like) {
	std::vector<Token> tokens = lexSynthetic(text);
	PPTok result = like;
	result.hide = nullptr;
	result.placemarker = false;
	result.tok = tokens.front();
	result.tok.spaceBefore = like.tok.spaceBefore;
	result.tok.startOfLine = like.tok.startOfLine;
	return result;
}

// ---------------------------------------------------------------------------
// Files and includes

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

// ---------------------------------------------------------------------------
// Directives

void Preprocessor::requireNoExtra(const std::vector<Token>& line, size_t used, const char* directive,
	const Cursor& cursor) const {

	if (line.size() > used) {
		fail(locate(cursor, line[used]), std::string("extra tokens at the end of #") + directive
			+ " (" + spell(line, used) + ")");
	}
}

void Preprocessor::directive(Cursor& cursor) {
	const SourceFile& file = cursor.file;
	const Token& hash = file.tokens[cursor.index];
	const Loc hashLoc = locate(cursor, hash);

	size_t end = cursor.index + 1;
	while (file.tokens[end].kind != TokenKind::EndOfFile && !file.tokens[end].startOfLine) {
		++end;
	}
	const std::vector<Token> line(file.tokens.begin() + cursor.index + 1, file.tokens.begin() + end);
	cursor.index = end;

	const bool skipping = !cursor.conditionals.empty() && !cursor.conditionals.back().live;

	// The null directive: a '#' alone on its line.
	if (line.empty()) {
		return;
	}

	if (!isName(line[0])) {
		if (skipping) {
			return;
		}
		fail(locate(cursor, line[0]), "expected a preprocessor directive after '#'");
	}

	const Directive kind = directiveByName(line[0].text);
	const Loc nameLoc = locate(cursor, line[0]);

	switch (kind) {
		case Directive::If:
		case Directive::Ifdef:
		case Directive::Ifndef: {
			if (cursor.conditionals.size() >= kMaxConditionalDepth) {
				fail(hashLoc, "conditionals nested more than " + std::to_string(kMaxConditionalDepth) + " deep");
			}

			Conditional conditional;
			conditional.opener = hashLoc;

			if (skipping) {
				conditional.live = false;
				conditional.taken = true;
			} else if (kind == Directive::If) {
				conditional.live = evaluateCondition(cursor, line, nameLoc);
				conditional.taken = conditional.live;
			} else {
				const char* directiveName = kind == Directive::Ifdef ? "ifdef" : "ifndef";
				if (line.size() < 2 || !isName(line[1])) {
					fail(nameLoc, std::string("#") + directiveName + " expects a macro name");
				}
				requireNoExtra(line, 2, directiveName, cursor);
				const bool defined = isDefinedName(line[1].text, locate(cursor, line[1]));
				conditional.live = kind == Directive::Ifdef ? defined : !defined;
				conditional.taken = conditional.live;
			}

			cursor.conditionals.push_back(conditional);
			return;
		}

		case Directive::Elif: {
			if (cursor.conditionals.empty()) {
				fail(nameLoc, "#elif without #if");
			}
			Conditional& top = cursor.conditionals.back();
			if (top.seenElse) {
				fail(nameLoc, "#elif after #else");
			}

			// Not evaluated unless it can be taken (a group inside a skipped one
			// starts out taken), so an expression only skipped text could make
			// sense of is not diagnosed.
			if (top.taken) {
				top.live = false;
			} else {
				top.live = evaluateCondition(cursor, line, nameLoc);
				top.taken = top.live;
			}
			return;
		}

		case Directive::Else: {
			if (cursor.conditionals.empty()) {
				fail(nameLoc, "#else without #if");
			}
			Conditional& top = cursor.conditionals.back();
			if (top.seenElse) {
				fail(nameLoc, "#else after #else");
			}
			top.seenElse = true;
			top.live = !top.taken;
			top.taken = true;
			return;
		}

		case Directive::Endif: {
			if (cursor.conditionals.empty()) {
				fail(nameLoc, "#endif without #if");
			}
			cursor.conditionals.pop_back();
			return;
		}

		default:
			break;
	}

	if (skipping) {
		return;
	}

	switch (kind) {
		case Directive::Define: defineMacro(cursor, line, nameLoc); return;
		case Directive::Undef: undefMacro(cursor, line, nameLoc); return;
		case Directive::Include: includeDirective(cursor, line, nameLoc, false); return;
		case Directive::Import: includeDirective(cursor, line, nameLoc, true); return;
		case Directive::Line: lineDirective(cursor, line, nameLoc); return;
		case Directive::Pragma: pragmaDirective(cursor, line, nameLoc); return;

		case Directive::Error:
			fail(nameLoc, line.size() > 1 ? "#error directive in this source: " + spell(line, 1)
				: std::string("#error directive in this source"));

		case Directive::Warning:
			// mslc has no channel for warnings, and Apple's compiler continues past
			// one, so the directive is accepted and has no effect.
			return;

		case Directive::Unsupported:
			fail(nameLoc, "unsupported preprocessor directive \"#" + std::string(line[0].text)
				+ "\"; mslc handles #include, #import, #define, #undef, #if, #ifdef, #ifndef, #elif, #else, "
				"#endif, #line, #error, #warning and #pragma");

		default:
			break;
	}

	fail(nameLoc, "unknown preprocessor directive \"#" + std::string(line[0].text)
		+ "\"; mslc handles #include, #import, #define, #undef, #if, #ifdef, #ifndef, #elif, #else, #endif, "
		"#line, #error, #warning and #pragma");
}

void Preprocessor::pragmaDirective(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	if (line.size() >= 2 && isName(line[1])) {
		if (line[1].text == "once") {
			cursor.file.once = true;
			return;
		}

		// These change which macros exist, so ignoring them would make a later
		// expansion differ from the reference compiler's.
		if (line[1].text == "push_macro" || line[1].text == "pop_macro") {
			fail(at, "#pragma " + std::string(line[1].text) + " is not supported");
		}
	}

	// A pragma is advice to the compiler. Apple's accepts one it does not know,
	// and so does mslc, but a pragma that mslc does not act on (pack, unroll,
	// clang diagnostic) is dropped, not honoured.
}

void Preprocessor::lineDirective(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	std::deque<PPTok> work;
	for (size_t i = 1; i < line.size(); ++i) {
		work.push_back(fromFile(cursor, line[i]));
	}
	std::vector<PPTok> expanded;
	scan(nullptr, work, expanded, 1);

	if (expanded.empty()) {
		fail(at, "#line directive requires a positive integer argument");
	}

	const Token& number = expanded[0].tok;
	if (number.kind != TokenKind::IntegerLiteral
		|| number.text.find_first_not_of("0123456789") != std::string_view::npos
		|| number.text.size() > 10) {
		fail(at, "#line directive requires a simple digit sequence");
	}
	const unsigned long long value = std::stoull(std::string(number.text));
	if (value > 2147483647ULL) {
		fail(at, "#line number is out of range");
	}

	size_t used = 1;
	if (expanded.size() >= 2) {
		if (!isStringLiteral(expanded[1].tok)) {
			fail(at, "invalid filename for #line directive");
		}
		const std::string_view quoted = expanded[1].tok.text;
		cursor.nameId = internName(std::string(quoted.substr(1, quoted.size() - 2)));
		used = 2;
	}
	if (expanded.size() > used) {
		fail(at, "extra tokens at the end of #line");
	}

	// The line after the directive is the one that is numbered.
	const Loc lastLoc = locate(cursor, line.back());
	cursor.lineDelta = static_cast<long>(value) - static_cast<long>(lastLoc.line + 1);
}

void Preprocessor::undefMacro(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	if (line.size() < 2 || !isName(line[1])) {
		fail(at, "macro name missing in #undef");
	}
	requireNoExtra(line, 2, "undef", cursor);

	const std::string_view name = line[1].text;
	if (isAlternativeOperator(name)) {
		fail(locate(cursor, line[1]), "C++ operator '" + std::string(name) + "' used as a macro name");
	}
	if (isBuiltinName(name)) {
		fail(locate(cursor, line[1]), "cannot #undef \"" + std::string(name) + "\": it is a builtin");
	}

	const auto it = _macros.find(name);
	if (it != _macros.end()) {
		_macros.erase(it);
	}
}

void Preprocessor::defineMacro(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	if (line.size() < 2 || !isName(line[1])) {
		fail(at, "macro name missing in #define");
	}

	const Token& nameToken = line[1];
	const Loc nameLoc = locate(cursor, nameToken);
	const std::string name(nameToken.text);

	if (isBuiltinName(name)) {
		fail(nameLoc, "cannot #define \"" + name + "\": it is a builtin");
	}
	if (isAlternativeOperator(name)) {
		fail(nameLoc, "C++ operator '" + name + "' used as a macro name");
	}
	if (isKeyword(name)) {
		fail(nameLoc, "cannot #define \"" + name + "\": it is a keyword, and redefining it would change "
			"what the rest of the source means");
	}

	auto macro = std::make_unique<Macro>();
	macro->name = name;
	macro->definedAt = nameLoc;

	std::map<std::string_view, size_t> parameterIndex;

	size_t bodyStart = 2;
	if (line.size() > 2 && line[2].kind == TokenKind::LParen && !line[2].spaceBefore) {
		macro->functionLike = true;
		size_t i = 3;
		bool closed = false;

		while (i < line.size()) {
			if (line[i].kind == TokenKind::RParen && macro->params.empty() && !macro->variadic) {
				closed = true;
				++i;
				break;
			}

			// "..." names the variable arguments __VA_ARGS__; "args..." names them args.
			const bool anonymousVariadic = line[i].kind == TokenKind::Ellipsis;
			if (!anonymousVariadic && !isName(line[i])) {
				fail(locate(cursor, line[i]), "expected a macro parameter name, found \""
					+ std::string(line[i].text) + "\"");
			}
			if (!anonymousVariadic && line[i].text == "__VA_ARGS__") {
				fail(locate(cursor, line[i]), "__VA_ARGS__ cannot be a macro parameter name");
			}
			if (!anonymousVariadic && !parameterIndex.emplace(line[i].text, macro->params.size()).second) {
				fail(locate(cursor, line[i]), "duplicate macro parameter \"" + std::string(line[i].text) + "\"");
			}

			if (anonymousVariadic) {
				macro->variadic = true;
				++i;
			} else if (i + 1 < line.size() && line[i + 1].kind == TokenKind::Ellipsis) {
				macro->variadic = true;
				macro->variadicName = std::string(line[i].text);
				parameterIndex.erase(line[i].text);
				i += 2;
			} else {
				macro->params.emplace_back(line[i].text);
				++i;
			}

			if (macro->variadic) {
				if (i >= line.size() || line[i].kind != TokenKind::RParen) {
					fail(locate(cursor, line[std::min(i, line.size() - 1)]),
						"expected ) to close a macro parameter list after the variable arguments");
				}
				closed = true;
				++i;
				break;
			}

			if (i < line.size() && line[i].kind == TokenKind::RParen) {
				closed = true;
				++i;
				break;
			}
			if (i < line.size() && line[i].kind == TokenKind::Comma) {
				++i;
				if (i < line.size() && line[i].kind == TokenKind::RParen) {
					fail(locate(cursor, line[i]), "expected a macro parameter name after ','");
				}
				continue;
			}
			break;
		}

		if (!closed) {
			fail(nameLoc, "expected ) to close a macro parameter list");
		}
		bodyStart = i;
	}

	std::vector<Token> body(line.begin() + bodyStart, line.end());
	for (const Token& token: body) {
		if (token.kind == TokenKind::Invalid) {
			fail(locate(cursor, token), describeInvalidToken(token));
		}
	}

	const size_t named = macro->params.size();
	auto paramIndex = [&](const Token& token) -> std::optional<size_t> {
		if (!macro->functionLike || !isName(token)) {
			return std::nullopt;
		}
		const auto found = parameterIndex.find(token.text);
		if (found != parameterIndex.end()) {
			return found->second;
		}
		if (macro->variadic && token.text == macro->variadicName) {
			return named;
		}
		return std::nullopt;
	};

	for (size_t i = 0; i < body.size(); ++i) {
		const Token& token = body[i];
		BodyItem item;
		item.tok = token;
		item.spaceBefore = i > 0 && token.spaceBefore;

		if (token.text == "__VA_ARGS__" && !(macro->variadic && macro->variadicName == "__VA_ARGS__")) {
			fail(locate(cursor, token), "__VA_ARGS__ can only appear in the expansion of a variadic macro");
		}
		if (token.kind == TokenKind::HashHash) {
			if (i == 0 || i + 1 == body.size()) {
				fail(locate(cursor, token), "'##' cannot appear at either end of a macro expansion");
			}
			if (body[i + 1].kind == TokenKind::HashHash) {
				fail(locate(cursor, token), "pasting formed '##', which is not a valid preprocessing token");
			}
			item.kind = BodyKind::Paste;
		} else if (macro->functionLike && token.kind == TokenKind::Hash) {
			if (i + 1 >= body.size() || !paramIndex(body[i + 1])) {
				fail(locate(cursor, token), "'#' is not followed by a macro parameter");
			}
			item.kind = BodyKind::Stringify;
			item.param = *paramIndex(body[i + 1]);
			++i;
		} else if (const auto index = paramIndex(token)) {
			item.kind = BodyKind::Param;
			item.param = *index;
		}

		macro->body.push_back(std::move(item));
	}

	const auto existing = _macros.find(name);
	if (existing != _macros.end()) {
		const Macro& old = *existing->second;
		bool same = old.functionLike == macro->functionLike && old.variadic == macro->variadic
			&& old.params == macro->params && old.variadicName == macro->variadicName
			&& old.body.size() == macro->body.size();
		for (size_t i = 0; same && i < old.body.size(); ++i) {
			const BodyItem& a = old.body[i];
			const BodyItem& b = macro->body[i];
			same = a.kind == b.kind && a.param == b.param && a.spaceBefore == b.spaceBefore
				&& (a.kind != BodyKind::Token || a.tok.text == b.tok.text);
		}
		if (!same) {
			fail(nameLoc, "\"" + name + "\" redefined differently (first defined at "
				+ describe(old.definedAt) + ")");
		}
		return;
	}

	_macros[name] = std::move(macro);
}

bool Preprocessor::isDefinedName(std::string_view name, const Loc& at) const {
	if (isUnsupportedBuiltin(name)) {
		fail(at, "\"" + std::string(name) + "\" is a builtin of Apple's compiler that mslc does not "
			"provide, so whether it is defined cannot be answered");
	}
	if (isAlternativeOperator(name)) {
		fail(at, "C++ operator '" + std::string(name) + "' used as a macro name");
	}
	return _macros.find(name) != _macros.end() || builtinByName(name) || name == "__has_include";
}

// ---------------------------------------------------------------------------
// #if

bool Preprocessor::evaluateCondition(Cursor& cursor, const std::vector<Token>& line, const Loc& at) {
	const size_t count = line.size();
	if (count < 2) {
		fail(at, "#if with no expression");
	}

	std::deque<PPTok> work;
	for (size_t i = 1; i < count;) {
		const Token& token = line[i];

		if (isName(token) && token.text == "defined") {
			size_t j = i + 1;
			bool parenthesised = false;
			if (j < count && line[j].kind == TokenKind::LParen) {
				parenthesised = true;
				++j;
			}
			if (j >= count || !isName(line[j])) {
				fail(locate(cursor, token), "operator 'defined' requires an identifier");
			}
			const bool defined = isDefinedName(line[j].text, locate(cursor, line[j]));
			++j;
			if (parenthesised) {
				if (j >= count || line[j].kind != TokenKind::RParen) {
					fail(locate(cursor, token), "missing ')' after 'defined'");
				}
				++j;
			}

			work.push_back(synthesize(defined ? "1" : "0", fromFile(cursor, token)));
			i = j;
			continue;
		}

		if (isName(token) && token.text == "__has_include") {
			if (i + 1 >= count || line[i + 1].kind != TokenKind::LParen) {
				fail(locate(cursor, token), "missing '(' after '__has_include'");
			}

			size_t close = i + 2;
			while (close < count && line[close].kind != TokenKind::RParen) {
				++close;
			}
			if (close >= count) {
				fail(locate(cursor, token), "missing ')' after '__has_include'");
			}

			const std::vector<Token> argument(line.begin() + i + 2, line.begin() + close);
			const Loc here = locate(cursor, token);
			const Header header = parseHeaderName(argument, 0, here, "#include");

			bool found = false;
			if (header.angled) {
				if (!builtinHeaderText(header.name)) {
					fail(here, "__has_include(" + header.name + ") cannot be answered: mslc provides "
						+ kBuiltinHeaderList + " only");
				}
				found = true;
			} else {
				found = resolveQuoted(cursor.file, header.name, here, "#include") != nullptr;
			}

			work.push_back(synthesize(found ? "1" : "0", fromFile(cursor, token)));
			i = close + 1;
			continue;
		}

		work.push_back(fromFile(cursor, token));
		++i;
	}

	Expression expr;
	scan(nullptr, work, expr.tokens, 1);
	expr.at = at;

	if (expr.tokens.empty()) {
		fail(at, "#if with no expression");
	}

	const Value value = parseTernary(expr, true);
	if (expr.position < expr.tokens.size()) {
		const PPTok& extra = expr.tokens[expr.position];
		if (extra.tok.kind == TokenKind::RParen) {
			fail(extra.loc, "unmatched ')' in the #if expression");
		}
		fail(extra.loc, "missing a binary operator before \"" + std::string(extra.tok.text)
			+ "\" in the #if expression");
	}
	return value.truthy();
}

Value Preprocessor::applyBinary(BinaryOp op, Value a, Value b, bool live, const Loc& at) const {
	Value result;
	const bool unsignedResult = a.isUnsigned || b.isUnsigned;
	result.isUnsigned = unsignedResult;

	auto boolean = [&](bool value) {
		Value v;
		v.bits = value ? 1 : 0;
		return v;
	};

	switch (op) {
		case BinaryOp::Mul:
			result.bits = a.bits * b.bits;
			return result;
		case BinaryOp::Add:
			result.bits = a.bits + b.bits;
			return result;
		case BinaryOp::Sub:
			result.bits = a.bits - b.bits;
			return result;

		case BinaryOp::Div:
		case BinaryOp::Mod: {
			if (b.bits == 0) {
				if (live) {
					fail(at, op == BinaryOp::Div ? "division by zero in the #if expression"
						: "remainder by zero in the #if expression");
				}
				return result;
			}
			if (unsignedResult) {
				result.bits = op == BinaryOp::Div ? a.bits / b.bits : a.bits % b.bits;
			} else if (a.bits == kSignBit && b.asSigned() == -1) {
				result.bits = op == BinaryOp::Div ? a.bits : 0;
			} else {
				result.bits = static_cast<Wide>(op == BinaryOp::Div ? a.asSigned() / b.asSigned()
					: a.asSigned() % b.asSigned());
			}
			return result;
		}

		case BinaryOp::Shl:
		case BinaryOp::Shr: {
			// The result has the left operand's type. The count is read as unsigned, so
			// a negative one is past the width like any other that is too large.
			result.isUnsigned = a.isUnsigned;
			const Wide count = b.bits;

			// Apple's compiler truncates a right-shift count to 32 bits first; mslc does not.
			if (op == BinaryOp::Shl) {
				result.bits = count >= kWideBits ? 0 : a.bits << static_cast<unsigned>(count);
			} else {
				// A right shift by the width or more leaves the sign bit, as it does in
				// Apple's compiler, rather than shifting everything out.
				const unsigned shift = count >= kWideBits ? kWideBits - 1 : static_cast<unsigned>(count);
				result.bits = a.isUnsigned ? a.bits >> shift : static_cast<Wide>(a.asSigned() >> shift);
			}
			return result;
		}

		case BinaryOp::BitAnd: result.bits = a.bits & b.bits; return result;
		case BinaryOp::BitXor: result.bits = a.bits ^ b.bits; return result;
		case BinaryOp::BitOr: result.bits = a.bits | b.bits; return result;

		case BinaryOp::Lt:
			return boolean(unsignedResult ? a.bits < b.bits : a.asSigned() < b.asSigned());
		case BinaryOp::Le:
			return boolean(unsignedResult ? a.bits <= b.bits : a.asSigned() <= b.asSigned());
		case BinaryOp::Gt:
			return boolean(unsignedResult ? a.bits > b.bits : a.asSigned() > b.asSigned());
		case BinaryOp::Ge:
			return boolean(unsignedResult ? a.bits >= b.bits : a.asSigned() >= b.asSigned());
		case BinaryOp::Eq: return boolean(a.bits == b.bits);
		case BinaryOp::Ne: return boolean(a.bits != b.bits);
		case BinaryOp::LogicalAnd: return boolean(a.truthy() && b.truthy());
		case BinaryOp::LogicalOr: return boolean(a.truthy() || b.truthy());
		case BinaryOp::None: break;
	}
	return result;
}

Value Preprocessor::parseTernary(Expression& expr, bool live) {
	if (++expr.depth > kMaxExpressionDepth) {
		fail(expr.at, "the #if expression is nested too deeply");
	}

	const Value condition = parseBinary(expr, 1, live);
	Value result = condition;

	if (expr.position < expr.tokens.size() && expr.tokens[expr.position].tok.kind == TokenKind::Question) {
		++expr.position;
		const Value whenTrue = parseTernary(expr, live && condition.truthy());

		if (expr.position >= expr.tokens.size() || expr.tokens[expr.position].tok.kind != TokenKind::Colon) {
			fail(expr.position < expr.tokens.size() ? expr.tokens[expr.position].loc : expr.at,
				"expected ':' in the #if conditional expression");
		}
		++expr.position;
		const Value whenFalse = parseTernary(expr, live && !condition.truthy());

		result = condition.truthy() ? whenTrue : whenFalse;
		result.isUnsigned = whenTrue.isUnsigned || whenFalse.isUnsigned;
	}

	--expr.depth;
	return result;
}

Value Preprocessor::parseBinary(Expression& expr, int minPrecedence, bool live) {
	Value lhs = parseUnary(expr, live);

	while (expr.position < expr.tokens.size()) {
		const PPTok& opToken = expr.tokens[expr.position];
		const BinaryOp op = binaryOpFor(opToken.tok);
		if (op == BinaryOp::None || precedence(op) < minPrecedence) {
			break;
		}
		++expr.position;

		// The right side of && and || is parsed but not evaluated when the left
		// decides the result, so a division by zero there is not an error.
		bool rhsLive = live;
		if (op == BinaryOp::LogicalAnd) {
			rhsLive = live && lhs.truthy();
		} else if (op == BinaryOp::LogicalOr) {
			rhsLive = live && !lhs.truthy();
		}

		const Value rhs = parseBinary(expr, precedence(op) + 1, rhsLive);
		lhs = applyBinary(op, lhs, rhs, live, opToken.loc);
	}

	return lhs;
}

Value Preprocessor::parseIntegerLiteral(Expression& expr) {
	const PPTok& first = expr.tokens[expr.position];
	std::string text(first.tok.text);
	++expr.position;

	// "1ull" and "0b101" arrive as a number followed by a name with no gap.
	while (expr.position < expr.tokens.size()) {
		const Token& next = expr.tokens[expr.position].tok;
		if (next.spaceBefore || !(isName(next) || next.kind == TokenKind::IntegerLiteral)) {
			break;
		}
		text += next.text;
		++expr.position;
	}

	size_t i = 0;
	unsigned base = 10;
	if (text.size() > 1 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
		base = 16;
		i = 2;
	} else if (text.size() > 1 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
		base = 2;
		i = 2;
	} else if (text.size() > 1 && text[0] == '0') {
		base = 8;
		i = 1;
	}

	Wide value = 0;
	bool any = base == 8;
	for (; i < text.size(); ++i) {
		const char c = text[i];
		unsigned digit = 0;
		if (c >= '0' && c <= '9') {
			digit = static_cast<unsigned>(c - '0');
		} else if (base == 16 && std::isxdigit(static_cast<unsigned char>(c))) {
			digit = static_cast<unsigned>(std::tolower(c) - 'a') + 10;
		} else {
			break;
		}

		if (digit >= base) {
			fail(first.loc, std::string("invalid digit '") + c + "' in " + (base == 8 ? "octal" : "binary")
				+ " constant");
		}
		if (value > (kWideMax - digit) / base) {
			fail(first.loc, "integer literal is too large to be represented in any integer type");
		}
		value = value * base + digit;
		any = true;
	}

	if (!any) {
		fail(first.loc, "invalid integer constant \"" + text + "\"");
	}

	std::string suffix = text.substr(i);
	std::transform(suffix.begin(), suffix.end(), suffix.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	static const char* const allowed[] = { "", "u", "l", "ul", "lu", "ll", "ull", "llu" };
	if (std::none_of(std::begin(allowed), std::end(allowed), [&](const char* s) { return suffix == s; })) {
		fail(first.loc, "invalid suffix \"" + text.substr(i) + "\" on integer constant");
	}

	Value result;
	result.bits = value;
	result.isUnsigned = suffix.find('u') != std::string::npos || value >= kSignBit;
	return result;
}

Value Preprocessor::parseUnary(Expression& expr, bool live) {
	if (expr.position >= expr.tokens.size()) {
		fail(expr.at, "expected a value in the #if expression");
	}

	if (++expr.depth > kMaxExpressionDepth) {
		fail(expr.at, "the #if expression is nested too deeply");
	}

	const PPTok& current = expr.tokens[expr.position];
	const Token& token = current.tok;
	Value result;

	auto unary = [&](auto apply) {
		++expr.position;
		Value operand = parseUnary(expr, live);
		result = apply(operand);
	};

	if (token.kind == TokenKind::Plus) {
		unary([](Value v) { return v; });
	} else if (token.kind == TokenKind::Minus) {
		unary([](Value v) { v.bits = 0 - v.bits; return v; });
	} else if (token.kind == TokenKind::Tilde || (isName(token) && token.text == "compl")) {
		unary([](Value v) { v.bits = ~v.bits; return v; });
	} else if (token.kind == TokenKind::Bang || (isName(token) && token.text == "not")) {
		unary([](Value v) { Value r; r.bits = v.truthy() ? 0 : 1; return r; });
	} else if (token.kind == TokenKind::LParen) {
		++expr.position;
		result = parseTernary(expr, live);

		// A comma inside parentheses evaluates both sides and yields the right.
		while (expr.position < expr.tokens.size() && expr.tokens[expr.position].tok.kind == TokenKind::Comma) {
			++expr.position;
			result = parseTernary(expr, live);
		}

		if (expr.position >= expr.tokens.size() || expr.tokens[expr.position].tok.kind != TokenKind::RParen) {
			fail(current.loc, "missing ')' in the #if expression");
		}
		++expr.position;
	} else if (token.kind == TokenKind::IntegerLiteral
		|| (token.kind == TokenKind::Invalid && token.invalidReason == InvalidReason::MalformedNumber)) {
		result = parseIntegerLiteral(expr);
	} else if (token.kind == TokenKind::FloatLiteral) {
		fail(current.loc, "floating constant in the #if expression");
	} else if (isName(token)) {
		if (token.text == "defined") {
			fail(current.loc, "'defined' produced by macro expansion is not supported");
		}

		// An identifier that is still here after expansion is not a macro, which
		// C reads as 0. true and false are C++ keywords with their own values.
		result.bits = token.text == "true" ? 1 : 0;
		++expr.position;
	} else if (token.kind == TokenKind::Invalid && token.text == "'") {
		fail(current.loc, "character constants in the #if expression are not supported");
	} else {
		fail(current.loc, "token \"" + std::string(token.text) + "\" is not valid in the #if expression");
	}

	--expr.depth;
	return result;
}

// ---------------------------------------------------------------------------
// Macro expansion

PPTok Preprocessor::stringify(const std::vector<PPTok>& argument, const PPTok& like, const Loc& at) {
	std::string text = "\"";

	// The lexer has no character literals, so a quote is tracked here: inside
	// one, as inside a string, a backslash or a double quote is escaped.
	bool inCharacter = false;
	for (size_t i = 0; i < argument.size(); ++i) {
		const Token& token = argument[i].tok;
		if (i > 0 && token.spaceBefore) {
			text += ' ';
		}

		const bool opensOrCloses = token.kind == TokenKind::Invalid && token.text == "'";
		if (opensOrCloses) {
			inCharacter = !inCharacter;
		}

		const bool quoted = isStringLiteral(token) || inCharacter || opensOrCloses
			|| (token.kind == TokenKind::Invalid && token.text[0] == '"');
		for (const char c: token.text) {
			if (quoted && (c == '"' || c == '\\')) {
				text += '\\';
			}
			text += c;
		}
	}
	text += '"';

	std::vector<Token> tokens = lexSynthetic(text);
	if (tokens.size() != 1 || tokens[0].kind == TokenKind::Invalid) {
		fail(at, "the argument cannot be stringified into a valid string literal");
	}

	PPTok result = like;
	result.hide = nullptr;
	result.placemarker = false;
	result.tok = tokens[0];
	result.tok.spaceBefore = like.tok.spaceBefore;
	result.tok.startOfLine = false;
	return result;
}

PPTok Preprocessor::pasteTokens(const PPTok& left, const PPTok& right, const Loc& at) {
	if (left.placemarker) {
		PPTok result = right;
		result.tok.spaceBefore = left.tok.spaceBefore;
		return result;
	}
	if (right.placemarker) {
		return left;
	}

	const std::string text = std::string(left.tok.text) + std::string(right.tok.text);
	std::vector<Token> tokens = lexSynthetic(text);

	bool valid = !tokens.empty();
	for (const Token& token: tokens) {
		valid = valid && token.kind != TokenKind::Invalid;
	}
	valid = valid && (tokens.size() == 1 || isPreprocessingNumber(text));
	if (!valid) {
		fail(at, "pasting \"" + std::string(left.tok.text) + "\" and \"" + std::string(right.tok.text)
			+ "\" does not give a valid preprocessing token");
	}

	// "2B" is one token in C, so it must not become the number 2 and the name B,
	// which could then be expanded. It is kept whole and diagnosed if it is used.
	if (tokens.size() > 1) {
		Token whole = tokens[0];
		whole.kind = TokenKind::Invalid;
		whole.invalidReason = InvalidReason::MalformedNumber;
		whole.text = _storage.back();
		tokens = { whole };
	}

	PPTok pasted = left;
	pasted.tok = tokens[0];
	pasted.tok.spaceBefore = left.tok.spaceBefore;
	pasted.tok.startOfLine = false;
	pasted.hide = hideSetIntersect(left.hide, right.hide);
	return pasted;
}

std::vector<PPTok> Preprocessor::substitute(const Macro& macro, const PPTok& name,
	const std::vector<std::vector<PPTok>>& arguments, bool variadicOmitted, const Loc& bodyLoc,
	size_t depth) {

	std::vector<std::optional<std::vector<PPTok>>> expandedArguments(arguments.size());
	auto expandedArgument = [&](size_t index) -> const std::vector<PPTok>& {
		if (!expandedArguments[index]) {
			std::deque<PPTok> work(arguments[index].begin(), arguments[index].end());
			std::vector<PPTok> out;
			scan(nullptr, work, out, depth + 1);
			expandedArguments[index] = std::move(out);
		}
		return *expandedArguments[index];
	};

	std::vector<PPTok> out;
	bool pasteNext = false;

	auto append = [&](std::vector<PPTok> contribution) {
		if (contribution.empty()) {
			return;
		}

		size_t from = 0;
		if (pasteNext) {
			pasteNext = false;
			out.back() = pasteTokens(out.back(), contribution[0], bodyLoc);
			from = 1;
		}

		for (size_t i = from; i < contribution.size(); ++i) {
			out.push_back(std::move(contribution[i]));
		}
	};

	auto bodyToken = [&](const BodyItem& item) {
		PPTok token;
		token.tok = item.tok;
		token.tok.spaceBefore = item.spaceBefore;
		token.tok.startOfLine = false;
		token.loc = bodyLoc;
		return token;
	};

	const std::vector<BodyItem>& body = macro.body;
	for (size_t k = 0; k < body.size(); ++k) {
		const BodyItem& item = body[k];

		switch (item.kind) {
			case BodyKind::Token:
				append({ bodyToken(item) });
				break;

			case BodyKind::Stringify:
				append({ stringify(arguments[item.param], bodyToken(item), bodyLoc) });
				break;

			case BodyKind::Paste: {
				// GNU: ", ## __VA_ARGS__" drops the comma when the variable arguments
				// are left out of the call, and otherwise pastes nothing. An empty one,
				// as in f(1,), keeps it, which is what Apple's compiler does.
				const bool commaBefore = !out.empty() && out.back().tok.kind == TokenKind::Comma
					&& !out.back().placemarker;
				const bool variadicNext = macro.variadic && k + 1 < body.size()
					&& body[k + 1].kind == BodyKind::Param && body[k + 1].param == macro.params.size();
				if (commaBefore && variadicNext && body[k - 1].kind == BodyKind::Token) {
					if (variadicOmitted) {
						out.pop_back();
						++k;
					}
					break;
				}
				pasteNext = true;
				break;
			}

			case BodyKind::Param: {
				const bool pasted = pasteNext || (k + 1 < body.size() && body[k + 1].kind == BodyKind::Paste);
				std::vector<PPTok> contribution = pasted ? arguments[item.param] : expandedArgument(item.param);

				if (contribution.empty()) {
					if (pasted) {
						PPTok marker;
						marker.placemarker = true;
						marker.tok.spaceBefore = item.spaceBefore;
						marker.loc = bodyLoc;
						contribution.push_back(std::move(marker));
					}
				} else {
					contribution[0].tok.spaceBefore = item.spaceBefore;
				}
				append(std::move(contribution));
				break;
			}
		}
	}

	out.erase(std::remove_if(out.begin(), out.end(), [](const PPTok& t) { return t.placemarker; }), out.end());

	if (!out.empty()) {
		out[0].tok.spaceBefore = name.tok.spaceBefore;
		out[0].tok.startOfLine = name.tok.startOfLine;
	}
	for (size_t i = 1; i < out.size(); ++i) {
		out[i].tok.startOfLine = false;
	}
	return out;
}

std::vector<PPTok> Preprocessor::expandBuiltin(Builtin builtin, const PPTok& name) {
	switch (builtin) {
		case Builtin::Line:
			return { synthesize(std::to_string(name.loc.presumedLine), name) };
		case Builtin::Counter:
			return { synthesize(std::to_string(_counter++), name) };
		case Builtin::IncludeLevel:
			return { synthesize(std::to_string(_includes.size()), name) };
		case Builtin::File:
		case Builtin::BaseFile: {
			const std::string& path = builtin == Builtin::File ? _names[name.loc.nameId] : _basePath;
			std::string literal = "\"";
			for (const char c: path) {
				if (c == '"' || c == '\\') {
					literal += '\\';
				}
				literal += c;
			}
			literal += '"';
			return { synthesize(literal, name) };
		}
	}
	return {};
}

void Preprocessor::scan(Cursor* cursor, std::deque<PPTok>& work, std::vector<PPTok>& out, size_t depth) {
	if (depth > kMaxExpansionDepth) {
		throw CompileError("macro arguments nested more than " + std::to_string(kMaxExpansionDepth)
			+ " deep");
	}

	// Pulls the next token for an argument list: pending rescan tokens first,
	// then the file. A directive inside an argument list is refused, because the
	// reference compiler's behaviour there is not portable.
	auto pull = [&](PPTok& token, const Loc& macroLoc, const std::string& macroName) {
		if (!work.empty()) {
			token = std::move(work.front());
			work.pop_front();
			return;
		}

		if (cursor) {
			const Token& raw = cursor->file.tokens[cursor->index];
			if (raw.kind == TokenKind::Hash && raw.startOfLine) {
				fail(locate(*cursor, raw), "a preprocessor directive inside the arguments of macro \""
					+ macroName + "\" is not supported");
			}
			if (raw.kind != TokenKind::EndOfFile) {
				token = fromFile(*cursor, raw);
				++cursor->index;
				return;
			}
		}

		fail(macroLoc, "unterminated argument list invoking macro \"" + macroName + "\"");
	};

	while (true) {
		PPTok token;

		// Handed over in batches so that one expansion that produces a great many
		// tokens reaches the output limit instead of exhausting memory first.
		if (cursor && out.size() >= 4096) {
			flush(out);
		}

		if (!work.empty()) {
			token = std::move(work.front());
			work.pop_front();
		} else {
			if (!cursor) {
				return;
			}

			flush(out);

			const Token& raw = cursor->file.tokens[cursor->index];
			if (raw.kind == TokenKind::EndOfFile) {
				return;
			}
			if (raw.kind == TokenKind::Hash && raw.startOfLine) {
				directive(*cursor);
				continue;
			}
			if (!cursor->conditionals.empty() && !cursor->conditionals.back().live) {
				++cursor->index;
				continue;
			}

			token = fromFile(*cursor, raw);
			++cursor->index;
		}

		if (!isName(token.tok)) {
			out.push_back(std::move(token));
			continue;
		}

		const std::string_view name = token.tok.text;

		if (const auto builtin = builtinByName(name)) {
			for (PPTok& expanded: expandBuiltin(*builtin, token)) {
				out.push_back(std::move(expanded));
			}
			continue;
		}
		if (name == "__has_include" || isUnsupportedBuiltin(name)) {
			fail(token.loc, name == "__has_include"
				? "__has_include can only be used in an #if or #elif"
				: "\"" + std::string(name) + "\" is a builtin of Apple's compiler that mslc does not provide");
		}

		const auto found = _macros.find(name);
		if (found == _macros.end() || hideSetHas(token.hide, found->second.get())) {
			out.push_back(std::move(token));
			continue;
		}

		const Macro& macro = *found->second;
		std::vector<std::vector<PPTok>> arguments;
		size_t heldHere = 0;
		bool variadicOmitted = false;
		HideSet hide;
		Loc bodyLoc = token.loc;

		if (!macro.functionLike) {
			hide = hideSetAdd(token.hide, &macro);
		} else {
			bool opens = false;
			if (!work.empty()) {
				opens = work.front().tok.kind == TokenKind::LParen;
			} else if (cursor) {
				opens = cursor->file.tokens[cursor->index].kind == TokenKind::LParen;
			}
			if (!opens) {
				out.push_back(std::move(token));
				continue;
			}

			PPTok open;
			pull(open, token.loc, macro.name);

			const size_t named = macro.params.size();
			arguments.emplace_back();
			size_t nesting = 0;
			PPTok close;

			while (true) {
				PPTok next;
				pull(next, token.loc, macro.name);

				// Every invocation being expanded keeps its arguments, and each nested one
				// copies what is inside it, so what is held at once is bounded as a whole:
				// by the time a nesting limit fires, the copies would already be too many.
				if (++_heldArguments > kMaxHeldArgumentTokens) {
					fail(token.loc, "the arguments of macro calls being expanded hold more than "
						+ std::to_string(kMaxHeldArgumentTokens) + " tokens");
				}
				++heldHere;

				if (next.tok.kind == TokenKind::LParen) {
					++nesting;
				} else if (next.tok.kind == TokenKind::RParen) {
					if (nesting == 0) {
						close = std::move(next);
						break;
					}
					--nesting;
				} else if (next.tok.kind == TokenKind::Comma && nesting == 0
					&& !(macro.variadic && arguments.size() == named + 1)) {
					arguments.emplace_back();
					continue;
				}

				arguments.back().push_back(std::move(next));
			}

			// f() is one empty argument to a macro that takes one, and none to a
			// macro that takes none or only variable arguments.
			if (named == 0 && arguments.size() == 1 && arguments[0].empty()) {
				arguments.clear();
			}
			if (macro.variadic && arguments.size() == named) {
				variadicOmitted = true;
				arguments.emplace_back();
			}

			const size_t wanted = named + (macro.variadic ? 1 : 0);
			if (arguments.size() != wanted) {
				fail(token.loc, "macro \"" + macro.name + "\" requires " + std::to_string(named)
					+ (macro.variadic ? " or more" : "") + " arguments, but " + std::to_string(arguments.size())
					+ (arguments.size() == 1 ? " was" : " were") + " given");
			}

			hide = hideSetAdd(hideSetIntersect(token.hide, close.hide), &macro);
			bodyLoc = close.loc;
		}

		std::vector<PPTok> result = substitute(macro, token, arguments, variadicOmitted, bodyLoc, depth);
		_heldArguments -= heldHere;

		_expanded += result.size();
		if (_expanded > kMaxExpandedTokens) {
			fail(token.loc, "macro expansion produces more than " + std::to_string(kMaxExpandedTokens)
				+ " tokens");
		}

		for (PPTok& t: result) {
			t.hide = hideSetUnion(t.hide, hide);
		}
		for (auto it = result.rbegin(); it != result.rend(); ++it) {
			work.push_front(std::move(*it));
		}

		if (work.size() + out.size() > kMaxPendingTokens) {
			fail(token.loc, "macro expansion holds more than " + std::to_string(kMaxPendingTokens)
				+ " tokens at once");
		}
	}
}

// ---------------------------------------------------------------------------
// Driver

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

}
