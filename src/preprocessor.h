#pragma once

#include "lexer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mslc {

struct PreprocessOptions {
	// Path of the source being preprocessed, for resolving a quoted #include
	// against its directory and for naming it in diagnostics. Empty when the
	// source did not come from a file: it can then include only from includeDirs.
	std::string sourcePath;

	// Searched, in order, after the including file's own directory. These and
	// the source's directory are the only places an #include may resolve to.
	std::vector<std::string> includeDirs;
};

// Where a token came from in the files the user wrote: for a token a macro produced,
// the place the macro was used.
struct TokenOrigin {
	uint32_t file = 0;
	uint32_t line = 0;
	uint32_t column = 0;
};

// The token stream the parser reads, with the text it points into.
struct PreprocessedSource {
	std::vector<Token> tokens;

	// One origin per token, and the names the origins index.
	std::vector<TokenOrigin> origins;
	std::vector<std::string> fileNames;

	// Owns the file texts and the text of tokens built by expansion. The tokens
	// are string_views into it, so they are valid only while this is alive.
	std::shared_ptr<void> storage;
};

// Runs the C preprocessor over MSL source: #define, #undef, conditionals,
// #include "file", #error, #line, #pragma, and macro expansion. The result has
// no directives left in it. Anything it cannot honour is a CompileError that
// names the construct and its file:line:col, never a directive that is dropped.
PreprocessedSource preprocess(std::string_view source, const PreprocessOptions& options);

// "file:line:col" of a token of a preprocessed source.
std::string describeOrigin(const PreprocessedSource& source, size_t tokenIndex);

// Rebuilds source text from preprocessed tokens, for `mslc -E`.
std::string renderTokens(const std::vector<Token>& tokens);

}
