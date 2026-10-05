// mslc command line driver.
//
// Thin wrapper over the C API, so the CLI exercises exactly the surface a
// Darling-side caller would.

#include "mslc/mslc.h"

#include "lexer.h"
#include "preprocessor.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace {

	bool readFile(const char* path, std::string& out) {
		FILE* handle = std::fopen(path, "rb");
		if (!handle) {
			return false;
		}

		std::fseek(handle, 0, SEEK_END);
		const long size = std::ftell(handle);
		if (size < 0) {
			std::fclose(handle);
			return false;
		}
		std::fseek(handle, 0, SEEK_SET);

		out.resize(static_cast<size_t>(size));
		const size_t read = out.empty() ? 0 : std::fread(&out[0], 1, out.size(), handle);
		std::fclose(handle);

		return read == out.size();
	}

	bool writeFile(const char* path, const void* data, size_t size) {
		FILE* handle = std::fopen(path, "wb");
		if (!handle) {
			return false;
		}

		const size_t written = size == 0 ? 0 : std::fwrite(data, 1, size, handle);
		std::fclose(handle);

		return written == size;
	}

	// A diagnostic that already starts with the input's path (the preprocessor
	// names file:line:col) is not prefixed with it a second time.
	void reportError(const char* input, const std::string& message) {
		const std::string located = std::string(input) + ":";
		if (message.compare(0, located.size(), located) == 0) {
			std::fprintf(stderr, "mslc: %s\n", message.c_str());
		} else {
			std::fprintf(stderr, "mslc: %s: %s\n", input, message.c_str());
		}
	}

	void usage() {
		std::fprintf(stderr,
			"usage: mslc [options] <input.metal>\n"
			"  -o, --output <path>   write SPIR-V here (default: alongside the input)\n"
			"      --reflect <path>  write the reflection JSON here\n"
			"      --stage <stage>   kernel, vertex or fragment; inferred when omitted\n"
			"      --local-size <x> <y> <z>  workgroup size for a kernel (default 1 1 1)\n"
			"  -V, --validate        run spirv-val on the result\n"
			"  -I <dir>              also search <dir> for a quoted #include (repeatable)\n"
			"  -E                    print the preprocessed source and exit\n"
			"      --dump-tokens   print the token stream and exit\n"
			"      --version         print the library version\n");
	}

	MslcStage parseStage(const char* text, bool& ok) {
		ok = true;
		if (std::strcmp(text, "kernel") == 0 || std::strcmp(text, "compute") == 0) {
			return MSLC_STAGE_KERNEL;
		}
		if (std::strcmp(text, "vertex") == 0) {
			return MSLC_STAGE_VERTEX;
		}
		if (std::strcmp(text, "fragment") == 0) {
			return MSLC_STAGE_FRAGMENT;
		}

		ok = false;
		return MSLC_STAGE_UNKNOWN;
	}

}

int main(int argc, char** argv) {
	MslcOptions options;
	mslc_default_options(&options);

	const char* input = nullptr;
	std::string output;
	std::string reflectionPath;
	bool validate = false;
	bool dumpTokens = false;
	bool preprocessOnly = false;
	std::vector<std::string> includeDirs;

	for (int i = 1; i < argc; ++i) {
		const std::string argument = argv[i];

		if (argument == "-o" || argument == "--output") {
			if (++i >= argc) { usage(); return 2; }
			output = argv[i];
		} else if (argument == "--reflect") {
			if (++i >= argc) { usage(); return 2; }
			reflectionPath = argv[i];
		} else if (argument == "--stage") {
			if (++i >= argc) { usage(); return 2; }
			bool ok = false;
			options.stage = parseStage(argv[i], ok);
			if (!ok) {
				std::fprintf(stderr, "mslc: unknown stage \"%s\"\n", argv[i]);
				return 2;
			}
		} else if (argument == "--local-size") {
			if (i + 3 >= argc) { usage(); return 2; }
			options.localSizeX = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
			options.localSizeY = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
			options.localSizeZ = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
		} else if (argument == "-V" || argument == "--validate") {
			validate = true;
		} else if (argument == "-I") {
			if (++i >= argc) { usage(); return 2; }
			includeDirs.push_back(argv[i]);
		} else if (argument.size() > 2 && argument.compare(0, 2, "-I") == 0) {
			includeDirs.push_back(argument.substr(2));
		} else if (argument == "-E") {
			preprocessOnly = true;
		} else if (argument == "--dump-tokens") {
			dumpTokens = true;
		} else if (argument == "--version") {
			std::printf("mslc %s\n", mslc_version());
			return 0;
		} else if (argument == "-h" || argument == "--help") {
			usage();
			return 0;
		} else if (!argument.empty() && argument[0] == '-') {
			std::fprintf(stderr, "mslc: unknown option \"%s\"\n", argument.c_str());
			usage();
			return 2;
		} else {
			input = argv[i];
		}
	}

	if (!input) {
		usage();
		return 2;
	}

	std::string source;
	if (!readFile(input, source)) {
		std::fprintf(stderr, "mslc: cannot read \"%s\"\n", input);
		return 2;
	}

	std::vector<const char*> includeDirPointers;
	for (const std::string& directory: includeDirs) {
		includeDirPointers.push_back(directory.c_str());
	}
	options.sourcePath = input;
	options.includeDirs = includeDirPointers.empty() ? nullptr : includeDirPointers.data();
	options.includeDirCount = includeDirPointers.size();

	if (preprocessOnly) {
		try {
			mslc::PreprocessOptions preprocessOptions;
			preprocessOptions.sourcePath = input;
			preprocessOptions.includeDirs = includeDirs;
			const mslc::PreprocessedSource result = mslc::preprocess(
				std::string_view(source.data(), source.size()), preprocessOptions);
			const std::string text = mslc::renderTokens(result.tokens);
			std::fwrite(text.data(), 1, text.size(), stdout);
			return 0;
		} catch (const mslc::CompileError& error) {
			reportError(input, error.what());
			return 1;
		}
	}

	if (dumpTokens) {
		// Useful when a parse error points somewhere surprising: it shows what
		// the lexer actually produced rather than what the source looks like.
		for (const mslc::Token& token: mslc::tokenize(std::string_view(source.data(), source.size()))) {
			std::printf("%6zu  %-20s %.*s\n", token.offset, mslc::tokenKindName(token.kind),
				static_cast<int>(token.text.size()), token.text.data());
		}
		return 0;
	}

	if (output.empty()) {
		output = input;
		const size_t dot = output.find_last_of('.');
		if (dot != std::string::npos) {
			output = output.substr(0, dot);
		}
		output += ".spv";
	}

	uint8_t* spirv = nullptr;
	size_t spirvSize = 0;
	char* reflection = nullptr;
	char* error = nullptr;

	const int result = mslc_translate(source.data(), source.size(), &options,
		&spirv, &spirvSize, reflectionPath.empty() ? nullptr : &reflection, &error);

	if (result != 0) {
		reportError(input, error ? error : "unknown error");
		mslc_free(error);
		mslc_free(reflection);
		return 1;
	}

	if (!writeFile(output.c_str(), spirv, spirvSize)) {
		std::fprintf(stderr, "mslc: cannot write \"%s\"\n", output.c_str());
		mslc_free(spirv);
		mslc_free(reflection);
		return 2;
	}

	if (!reflectionPath.empty() && reflection) {
		if (!writeFile(reflectionPath.c_str(), reflection, std::strlen(reflection))) {
			std::fprintf(stderr, "mslc: cannot write \"%s\"\n", reflectionPath.c_str());
		}
	}

	if (validate) {
		// Run spirv-val as a separate step rather than linking SPIRV-Tools
		// into the library, so a build without it still works. Spawned without
		// a shell so the output path needs no quoting.
		const char* arguments[] = { "spirv-val", "--target-env", "vulkan1.3", output.c_str(), nullptr };
		pid_t child = 0;
		int status = 0;
		const bool ran = posix_spawnp(&child, "spirv-val", nullptr, nullptr,
			const_cast<char* const*>(arguments), environ) == 0
			&& waitpid(child, &status, 0) == child;
		const bool passed = ran && WIFEXITED(status) && WEXITSTATUS(status) == 0;
		std::printf("spirv-val: %s\n", passed ? "PASS" : ran ? "FAIL" : "could not run spirv-val");
		if (!passed) {
			mslc_free(spirv);
			mslc_free(reflection);
			return 1;
		}
	}

	std::printf("mslc: wrote %s (%zu bytes)\n", output.c_str(), spirvSize);

	mslc_free(spirv);
	mslc_free(reflection);

	return 0;
}
