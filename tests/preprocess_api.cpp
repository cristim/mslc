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
		translate("#include \"../pp_include_relative_header.metal\"\n" + kernel, options), "resolves outside");

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

	// A NUL inside the name would end it early in anything that took it as a C string.
	mslc_default_options(&options);
	options.includeDirs = directories;
	options.includeDirCount = 1;
	expectError("NUL in an include name",
		translate(std::string("#include \"pp_defs.h\0evil\"\n", 26) + kernel, options), "NUL byte");

	const char* empty[] = { "" };
	mslc_default_options(&options);
	options.includeDirs = empty;
	options.includeDirCount = 1;
	expectError("empty include directory", translate("#define VALUE 1\n" + kernel, options), "empty string");

	// Predefined macros: a value, a missing value (1), an empty value, and a source that defines the
	// name again.
	const std::string useValue = "kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])\n"
		"{ out[i] = VALUE; }\n";
	const char* names[] = { "VALUE" };
	const char* values[] = { "7" };
	mslc_default_options(&options);
	options.macroNames = names;
	options.macroValues = values;
	options.macroCount = 1;
	expectSuccess("a predefined macro", translate(useValue, options));
	expectSuccess("a predefined macro the source defines again", translate("#define VALUE 9\n" + useValue, options));
	expectError("a predefined macro is visible to the source",
		translate("#if VALUE == 7\n#error saw seven\n#endif\n" + useValue, options), "saw seven");

	options.macroValues = nullptr;
	expectSuccess("a predefined macro with no values is 1",
		translate("#if VALUE != 1\n#error not one\n#endif\n" + useValue, options));
	const char* nullValue[] = { nullptr };
	options.macroValues = nullValue;
	expectSuccess("a predefined macro with a NULL value is 1",
		translate("#if VALUE != 1\n#error not one\n#endif\n" + useValue, options));

	const char* emptyValue[] = { "" };
	options.macroValues = emptyValue;
	expectSuccess("a predefined macro with an empty value is defined",
		translate("#ifndef VALUE\n#error not defined\n#endif\nkernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])\n{ out[i] = VALUE 5; }\n", options));

	const char* lineBreak[] = { "1\n#error injected" };
	options.macroValues = lineBreak;
	expectError("a line break in a value", translate(useValue, options), "line break");

	const char* badNames[] = { "NOT-A-NAME" };
	options.macroNames = badNames;
	options.macroValues = values;
	expectError("a name that is not an identifier", translate(useValue, options), "not an identifier");

	const char* nullNames[] = { nullptr };
	options.macroNames = nullNames;
	expectError("a NULL name", translate(useValue, options), "NULL entry");

	mslc_default_options(&options);
	options.macroCount = 1;
	expectError("a count without names", translate(useValue, options), "macroNames");

	if (failures == 0) {
		std::printf("mslc-api-test: all passed\n");
	}
	return failures == 0 ? 0 : 1;
}
