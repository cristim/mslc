#pragma once

#include "lexer.h"

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

// The token stream the parser reads, with the text it points into.
struct PreprocessedSource {
	std::vector<Token> tokens;

	// Owns the file texts and the text of tokens built by expansion. The tokens
	// are string_views into it, so they are valid only while this is alive.
	std::shared_ptr<void> storage;
};

// Runs the C preprocessor over MSL source: #define, #undef, conditionals,
// #include "file", #error, #line, #pragma, and macro expansion. The result has
// no directives left in it. Anything it cannot honour is a CompileError that
// names the construct and its file:line:col, never a directive that is dropped.
PreprocessedSource preprocess(std::string_view source, const PreprocessOptions& options);

// Rebuilds source text from preprocessed tokens, for `mslc -E`.
std::string renderTokens(const std::vector<Token>& tokens);

}
