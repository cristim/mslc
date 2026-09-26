// mslc command line driver.
//
// Thin wrapper over the C API, so the CLI exercises exactly the surface a
// Darling-side caller would.

#include "mslc/mslc.h"

#include "lexer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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

	void usage() {
		std::fprintf(stderr,
			"usage: mslc [options] <input.metal>\n"
			"  -o, --output <path>   write SPIR-V here (default: alongside the input)\n"
			"      --reflect <path>  write the reflection JSON here\n"
			"      --stage <stage>   kernel, vertex or fragment; inferred when omitted\n"
			"      --local-size <x> <y> <z>  workgroup size for a kernel (default 1 1 1)\n"
			"  -V, --validate        run spirv-val on the result\n"
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
		std::fprintf(stderr, "mslc: %s: %s\n", input, error ? error : "unknown error");
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
		// into the library, so a build without it still works.
		std::string command = "spirv-val --target-env vulkan1.2 " + output
			+ " >/dev/null 2>&1 && echo PASS || echo FAIL";
		FILE* pipe = popen(command.c_str(), "r");
		char verdict[16] = { 0 };
		if (pipe && std::fgets(verdict, sizeof(verdict), pipe)) {
			std::printf("spirv-val: %s", verdict);
		}
		if (pipe) {
			pclose(pipe);
		}
	}

	std::printf("mslc: wrote %s (%zu bytes)\n", output.c_str(), spirvSize);

	mslc_free(spirv);
	mslc_free(reflection);

	return 0;
}
