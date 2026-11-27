#pragma once

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

#include "preprocessor.h"

namespace mslc {

namespace fs = std::filesystem;

namespace pp {

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

constexpr char kBuiltinHeaderList[] = "<metal_stdlib>, <metal_matrix>, <simd/simd.h>, <simd/matrix_types.h>, "
	"<simd/packed.h>, <simd/vector_types.h> and the <metal_stdlib> sub-headers <metal_texture>, "
	"<metal_math>, <metal_common>, <metal_geometric>, <metal_integer>, <metal_relational>, "
	"<metal_graphics>, <metal_types>, <metal_atomic> and <metal_pack>";

const std::string& builtinHeaderKey(const std::string& name);
const char* builtinHeaderText(const std::string& rawName);

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

bool hideSetHas(const HideSet& set, const Macro* macro);
HideSet hideSetAdd(const HideSet& set, const Macro* macro);
HideSet hideSetUnion(const HideSet& a, const HideSet& b);
HideSet hideSetIntersect(const HideSet& a, const HideSet& b);

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
	// defined by PreprocessOptions::macros rather than by the source: a #define of the same name in
	// the source replaces it, as it does on a compiler's command line, instead of conflicting
	bool predefined = false;
};

enum class Builtin { Line, File, BaseFile, IncludeLevel, Counter };

std::optional<Builtin> builtinByName(std::string_view name);
bool isUnsupportedBuiltin(std::string_view name);
bool isBuiltinName(std::string_view name);
bool isAlternativeOperator(std::string_view name);
bool isKeyword(std::string_view name);

inline bool isName(const Token& token) {
	return token.kind == TokenKind::Identifier;
}

inline bool isStringLiteral(const Token& token) {
	return token.kind == TokenKind::StringLiteral;
}

// The tokens' text, with a space where the source had whitespace.
inline std::string spell(const std::vector<Token>& tokens, size_t first = 0) {
	std::string text;
	for (size_t i = first; i < tokens.size(); ++i) {
		if (i > first && tokens[i].spaceBefore) {
			text += ' ';
		}
		text += tokens[i].text;
	}
	return text;
}

bool isPreprocessingNumber(std::string_view text);
void spliceLines(std::string_view raw, std::string& text, std::vector<size_t>& lineStarts);
bool isWithin(const fs::path& path, const fs::path& root);

enum class Directive {
	Define, Undef, Include, Import, If, Ifdef, Ifndef, Elif, Else, Endif, Line, Error, Warning,
	Pragma, Unsupported, Unknown,
};

Directive directiveByName(std::string_view name);

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

int precedence(BinaryOp op);
BinaryOp binaryOpFor(const Token& token);

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

} // namespace pp

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
	std::map<const pp::SourceFile*, uint32_t> _fileIndex;
	std::map<std::string, pp::SourceFile*> _builtinHeaders;
	std::deque<std::string> _storage;
	std::vector<std::unique_ptr<pp::SourceFile>> _owned;
	std::map<std::string, pp::SourceFile*> _files;
	std::map<std::string, std::unique_ptr<pp::Macro>, std::less<>> _macros;
	std::vector<std::string> _names;
	std::vector<fs::path> _roots;
	std::vector<pp::Loc> _includes;
	std::string _basePath;
	// set while the macros of PreprocessOptions::macros are being defined
	bool _definingPredefined = false;
	size_t _counter = 0;
	size_t _includeCount = 0;
	size_t _expanded = 0;
	size_t _heldArguments = 0;

	[[noreturn]] void fail(const pp::Loc& loc, const std::string& message) const;
	std::string describe(const pp::Loc& loc) const;

	pp::SourceFile& loadText(std::string displayPath, std::string canonical, std::string_view raw);
	size_t internName(const std::string& name);
	pp::Loc locate(const pp::Cursor& cursor, const Token& token) const;
	pp::PPTok fromFile(const pp::Cursor& cursor, const Token& token) const;

	void processFile(pp::SourceFile& file, size_t nameId);
	void scan(pp::Cursor* cursor, std::deque<pp::PPTok>& work, std::vector<pp::PPTok>& out, size_t depth,
		bool keepPragma = false);
	void flush(std::vector<pp::PPTok>& out);
	TokenOrigin originOf(const pp::Loc& loc);

	// Directives
	void directive(pp::Cursor& cursor);
	void defineMacro(pp::Cursor& cursor, const std::vector<Token>& line, const pp::Loc& at);
	void undefMacro(pp::Cursor& cursor, const std::vector<Token>& line, const pp::Loc& at);
	bool admit(pp::SourceFile& file, bool import);
	void includeDirective(pp::Cursor& cursor, const std::vector<Token>& line, const pp::Loc& at, bool import);
	void lineDirective(pp::Cursor& cursor, const std::vector<Token>& line, const pp::Loc& at);
	void pragmaDirective(pp::Cursor& cursor, const std::vector<Token>& line, const pp::Loc& at);
	void applyPragma(pp::Cursor* cursor, std::string_view first, const pp::Loc& at);
	bool evaluateCondition(pp::Cursor& cursor, const std::vector<Token>& line, const pp::Loc& at);
	void requireNoExtra(const std::vector<Token>& line, size_t used, const char* directive,
		const pp::Cursor& cursor) const;

	// Includes
	struct Header {
		bool angled = false;
		std::string name;
	};
	Header parseHeaderName(const std::vector<Token>& line, size_t first, const pp::Loc& at,
		const std::string& directive) const;
	pp::SourceFile* resolveQuoted(const pp::SourceFile& includer, const std::string& name, const pp::Loc& at,
		const std::string& directive);
	pp::SourceFile* loadFile(const fs::path& path, const std::string& displayPath, const pp::Loc& at,
		const std::string& directive);

	// Macro expansion
	bool isDefinedName(std::string_view name, const pp::Loc& at) const;
	pp::PPTok synthesize(const std::string& text, const pp::PPTok& like);
	std::vector<Token> lexSynthetic(const std::string& text);
	pp::PPTok pasteTokens(const pp::PPTok& left, const pp::PPTok& right, const pp::Loc& at);
	pp::PPTok stringify(const std::vector<pp::PPTok>& argument, const pp::PPTok& like, const pp::Loc& at);
	std::vector<pp::PPTok> substitute(const pp::Macro& macro, const pp::PPTok& name,
		const std::vector<std::vector<pp::PPTok>>& arguments, bool variadicOmitted, const pp::Loc& bodyLoc,
		size_t depth);
	std::vector<pp::PPTok> expandBuiltin(pp::Builtin builtin, const pp::PPTok& name);

	// #if expressions
	pp::Value parseTernary(pp::Expression& expr, bool live);
	pp::Value parseBinary(pp::Expression& expr, int minPrecedence, bool live);
	pp::Value parseUnary(pp::Expression& expr, bool live);
	pp::Value parseIntegerLiteral(pp::Expression& expr);
	pp::Value applyBinary(pp::BinaryOp op, pp::Value a, pp::Value b, bool live, const pp::Loc& at) const;
};

} // namespace mslc
