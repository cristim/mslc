// The library entry point's preprocessing options, which the CLI always sets
// together and so cannot reach separately: a source that is only a string, an
// include directory list on its own, and the checks on both.

#include "mslc/mslc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

	int failures = 0;

	// Translates source and returns the diagnostic, empty on success.
	std::string translate(const std::string& source, const MslcOptions& options) {
		uint8_t* spirv = nullptr;
		size_t size = 0;
		char* error = nullptr;
		const int status = mslc_translate(source.data(), source.size(), &options, &spirv, &size, nullptr, &error);

		std::string message = error ? error : "";
		mslc_free(error);
		mslc_free(spirv);

		if (status != 0 && message.empty()) {
			message = "failed with no diagnostic";
		}
		return message;
	}

	void expectSuccess(const char* name, const std::string& message) {
		if (!message.empty()) {
			std::printf("FAIL %s: expected success, got: %s\n", name, message.c_str());
			++failures;
		}
	}

	void expectError(const char* name, const std::string& message, const char* needle) {
		if (message.find(needle) == std::string::npos) {
			std::printf("FAIL %s: expected \"%s\", got: %s\n", name, needle, message.empty() ? "success" : message.c_str());
			++failures;
		}
	}

}

int main(int argc, char** argv) {
	if (argc != 2) {
		std::fprintf(stderr, "usage: mslc-api-test <directory holding pp_defs.h>\n");
		return 2;
	}
	const std::string includeDirectory = argv[1];

	const std::string kernel = "kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])\n"
		"{ out[i] = VALUE; }\n";

	MslcOptions options;

	// A source with no path still has macros and conditionals.
	mslc_default_options(&options);
	expectSuccess("string source with macros", translate("#define BASE 3\n#if BASE > 2\n#define VALUE BASE\n#endif\n" + kernel, options));

	// A quoted include in it has nowhere to be relative to.
	expectError("string source, include with no directories",
		translate("#include \"pp_defs.h\"\n#define VALUE PP_HEADER_VALUE\n" + kernel, options), "\"pp_defs.h\" not found");

	// It resolves against the include directories alone.
	const char* directories[] = { includeDirectory.c_str() };
	options.includeDirs = directories;
	options.includeDirCount = 1;
	expectSuccess("string source, include directory",
		translate("#include \"pp_defs.h\"\n#define VALUE PP_HEADER_VALUE\n" + kernel, options));

	// A source path need not exist: it names the directory includes are relative to.
	mslc_default_options(&options);
	const std::string virtualSource = includeDirectory + "/not-a-real-file.metal";
	options.sourcePath = virtualSource.c_str();
	expectSuccess("source path, relative include",
		translate("#include \"pp_defs.h\"\n#define VALUE PP_HEADER_VALUE\n" + kernel, options));

	// ... and bounds them: this reaches a real file beside the directory.
	expectError("source path, include that escapes",
		translate("#include \"../float_add.metal\"\n" + kernel, options), "resolves outside");

	// An include directory that is not there is refused, not skipped.
	const char* missing[] = { "/nonexistent/include/directory" };
	mslc_default_options(&options);
	options.includeDirs = missing;
	options.includeDirCount = 1;
	expectError("missing include directory", translate("#define VALUE 1\n" + kernel, options), "does not exist");

	// A count with no array behind it is a caller bug and is named as one.
	mslc_default_options(&options);
	options.includeDirCount = 2;
	expectError("count without array", translate("#define VALUE 1\n" + kernel, options), "includeDirs");

	const char* withNull[] = { includeDirectory.c_str(), nullptr };
	mslc_default_options(&options);
	options.includeDirs = withNull;
	options.includeDirCount = 2;
	expectError("null entry", translate("#define VALUE 1\n" + kernel, options), "NULL entry");

	if (failures == 0) {
		std::printf("mslc-api-test: all passed\n");
	}
	return failures == 0 ? 0 : 1;
}
