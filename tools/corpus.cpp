// Corpus runner: compiles every .metal file listed in a manifest and reports
// which reach structurally valid SPIR-V.
//
// The count is the measurement, not the assertion: progress on the MSL subset
// is "N of these files compile", not a claim. The assertion is per entry, and
// the manifest says what is expected of each one, so a fixture mslc supports
// has to keep compiling and one it does not support yet has to keep saying why.
// That way the test cannot pass by having fewer fixtures in it, and progress
// has to be recorded in the manifest rather than absorbed by it.

#include "mslc/mslc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

	// What a manifest entry says should happen to its file.
	struct Expectation {
		// The file has to compile and reach spirv-val-valid SPIR-V.
		bool valid = false;

		// The file has to compile, and spirv-val has to reject the result with a
		// diagnostic naming this. A construct mslc lowers to something Vulkan's own
		// rules refuse is a third thing between the two: the source is understood
		// and the module is emitted, but the module is not one a pipeline would
		// accept. That still has to be asserted, and asserted by name, or the entry
		// cannot fail in either direction.
		bool invalid = false;

		// The file has to fail to compile, and the diagnostic has to mention this.
		// Empty when it is expected to compile.
		std::string blocker;
	};

	struct Result {
		std::string path;
		bool compiled = false;
		bool valid = false;
		size_t spirvSize = 0;
		std::string diagnostic;
		bool met = false;
		std::string note;
	};

	bool readFile(const std::string& path, std::string& out) {
		std::ifstream stream(path, std::ios::binary);
		if (!stream) {
			return false;
		}

		out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
		return true;
	}

	// A relative manifest entry names a file next to the manifest, not one
	// relative to whatever directory the runner happens to be started from.
	std::string resolve(const std::string& manifestPath, const std::string& entry) {
		if (entry.empty() || entry[0] == '/') {
			return entry;
		}

		const size_t slash = manifestPath.find_last_of('/');
		return slash == std::string::npos ? entry : manifestPath.substr(0, slash + 1) + entry;
	}

	bool validateWithSpirvVal(const std::string& spvPath, std::string& detail) {
		const std::string command = "spirv-val --target-env vulkan1.2 " + spvPath
			+ " 2>&1 >/dev/null";
		FILE* pipe = popen(command.c_str(), "r");
		if (!pipe) {
			detail = "could not run spirv-val";
			return false;
		}

		char buffer[512];
		while (std::fgets(buffer, sizeof(buffer), pipe)) {
			if (!detail.empty()) {
				detail += "; ";
			}
			detail += buffer;
		}

		const int status = pclose(pipe);
		return status == 0;
	}

	// Reads a manifest entry: a path, then what is expected of it. An entry
	// without an expectation is an error rather than a fixture measured and
	// forgotten, since an unasserted entry is one that cannot fail.
	// The expectation runs to the end of the line, so the text of a blocker may
	// contain spaces. Returns false when the line cannot be read.
	bool parseEntry(const std::string& line, std::string& outPath, Expectation& outExpectation,
		std::string& outError) {

		std::istringstream stream(line);
		if (!(stream >> outPath)) {
			outError = "expected a path";
			return false;
		}

		const size_t at = line.find("expect=");
		if (at == std::string::npos) {
			outError = "no expectation given, so the entry could not fail";
			return false;
		}

		const std::string value = line.substr(at + std::string("expect=").size());
		if (value == "valid") {
			outExpectation.valid = true;
			return true;
		}

		constexpr const char* kUnsupported = "unsupported:";
		if (value.rfind(kUnsupported, 0) == 0) {
			// A fixture mslc cannot handle has to say which construct is in the
			// way, so that moving the frontier is a deliberate edit to this file
			// rather than a fixture quietly going green.
			outExpectation.blocker = value.substr(std::string(kUnsupported).size());
			if (outExpectation.blocker.empty()) {
				outError = "expect=unsupported: has to name what is in the way";
				return false;
			}

			return true;
		}

		constexpr const char* kInvalid = "invalid:";
		if (value.rfind(kInvalid, 0) == 0) {
			outExpectation.invalid = true;
			outExpectation.blocker = value.substr(std::string(kInvalid).size());
			if (outExpectation.blocker.empty()) {
				outError = "expect=invalid: has to name why the module is rejected";
				return false;
			}

			return true;
		}

		outError = "expect= takes valid, unsupported:<what is in the way> or "
			"invalid:<why spirv-val rejects the module>, found " + value;
		return false;
	}

	// A name for the scratch file that says which fixture it is. Three of the
	// corpus files are called shaders.metal, so the basename alone would have
	// them overwrite each other.
	std::string scratchName(const std::string& path) {
		std::string name = path;
		for (char& c: name) {
			if (c == '/' || c == '.') {
				c = '_';
			}
		}

		return name;
	}

}

int main(int argc, char** argv) {
	if (argc < 2) {
		std::fprintf(stderr, "usage: mslc-corpus <manifest> [--keep-output <dir>]\n");
		return 2;
	}

	const std::string manifestPath = argv[1];

	// Default into TMPDIR rather than a fixed /tmp path, so the scratch output
	// follows the caller's idea of where temporary files belong.
	const char* tmpdir = std::getenv("TMPDIR");
	std::string outputDirectory = std::string(tmpdir && *tmpdir ? tmpdir : "/tmp") + "/mslc-corpus";

	// Without this a missing directory turns every shader into "INVALID", which
	// reads like a compiler regression rather than a missing scratch dir.
	std::error_code ec;
	std::filesystem::create_directories(outputDirectory, ec);
	if (ec) {
		std::fprintf(stderr, "mslc-corpus: cannot create %s: %s\n", outputDirectory.c_str(), ec.message().c_str());
		return 2;
	}

	for (int i = 2; i + 1 < argc; ++i) {
		if (std::strcmp(argv[i], "--keep-output") == 0) {
			outputDirectory = argv[i + 1];
		}
	}

	std::ifstream manifest(manifestPath);
	if (!manifest) {
		std::fprintf(stderr, "mslc-corpus: cannot read %s\n", manifestPath.c_str());
		return 2;
	}

	std::vector<Result> results;
	std::string line;
	size_t lineNumber = 0;
	while (std::getline(manifest, line)) {
		++lineNumber;
		if (line.empty() || line[0] == '#') {
			continue;
		}

		Result result;
		Expectation expectation;
		std::string problem;
		std::string entry;
		if (!parseEntry(line, entry, expectation, problem)) {
			std::fprintf(stderr, "mslc-corpus: %s:%zu: %s\n", manifestPath.c_str(),
				lineNumber, problem.c_str());
			return 2;
		}

		result.path = resolve(manifestPath, entry);

		std::string source;
		if (!readFile(result.path, source)) {
			result.diagnostic = "cannot read file";
			result.note = "the file could not be read, so nothing was checked";
			results.push_back(result);
			continue;
		}

		MslcOptions options;
		mslc_default_options(&options);

		uint8_t* spirv = nullptr;
		size_t spirvSize = 0;
		char* error = nullptr;

		const int status = mslc_translate(source.data(), source.size(), &options,
			&spirv, &spirvSize, nullptr, &error);

		if (status != 0) {
			result.diagnostic = error ? error : "unknown error";
			mslc_free(error);

			// Expected to fail: the diagnostic has to name the construct that is in
			// the way, so a fixture that starts failing for a different reason, or
			// stops failing at all, is visible here. A fixture expected to compile
			// cannot be met by failing.
			result.met = !expectation.valid && !expectation.invalid
				&& result.diagnostic.find(expectation.blocker) != std::string::npos;
			if (!result.met) {
				if (expectation.valid) {
					result.note = "expected to compile";
				} else if (expectation.invalid) {
					result.note = "expected to compile and then be rejected because \""
						+ expectation.blocker + "\", but it did not compile: "
						+ result.diagnostic;
				} else {
					result.note = "expected to be blocked on \"" + expectation.blocker + "\"";
				}
			}

			results.push_back(result);
			continue;
		}

		result.compiled = true;
		result.spirvSize = spirvSize;

		// Write the module out so spirv-val can check it, which is a stronger
		// statement than "the compiler returned success".
		const std::string spvPath = outputDirectory + "/" + scratchName(result.path) + ".spv";

		FILE* out = std::fopen(spvPath.c_str(), "wb");
		if (out) {
			std::fwrite(spirv, 1, spirvSize, out);
			std::fclose(out);

			std::string detail;
			result.valid = validateWithSpirvVal(spvPath, detail);
			if (!result.valid) {
				result.diagnostic = detail;
			}
		} else {
			result.diagnostic = "cannot write " + spvPath;
		}

		// A fixture expected to compile has to reach valid SPIR-V, not merely
		// return success: compiling to something spirv-val rejects is a failure.
		// One expected to be rejected has to be rejected for the named reason,
		// which is the same assertion pointed the other way: a fix that makes it
		// valid is as visible here as a regression that changes the reason.
		if (expectation.valid) {
			result.met = result.valid;
			if (!result.met) {
				result.note = "expected to compile";
			}
		} else if (expectation.invalid) {
			result.met = !result.valid
				&& result.diagnostic.find(expectation.blocker) != std::string::npos;
			if (!result.met) {
				result.note = "expected to be rejected because \"" + expectation.blocker
					+ (result.valid ? "\", but it is valid" : "\", but not for that reason");
			}
		} else {
			result.met = false;
			result.note = "expected to be blocked on \"" + expectation.blocker
				+ "\", but it compiled";
		}

		mslc_free(spirv);
		results.push_back(result);
	}

	size_t compiled = 0;
	size_t valid = 0;
	size_t unmet = 0;

	for (const Result& result: results) {
		std::printf("%-34s ", result.path.substr(result.path.find_last_of('/') + 1).c_str());

		if (result.valid) {
			std::printf("VALID   %6zu bytes\n", result.spirvSize);
			++valid;
			++compiled;
		} else if (result.compiled) {
			std::printf("INVALID %6zu bytes  %s\n", result.spirvSize,
				result.diagnostic.empty() ? "" : result.diagnostic.c_str());
			++compiled;
		} else {
			std::printf("FAIL    %s\n", result.diagnostic.c_str());
		}

		if (!result.met) {
			++unmet;
			std::printf("%-34s   UNMET: %s\n", "", result.note.c_str());
		}
	}

	std::printf("\n%zu of %zu compile, %zu of %zu reach spirv-val-valid SPIR-V\n",
		compiled, results.size(), valid, results.size());

	if (unmet == 0) {
		std::printf("every entry is as the manifest says it is\n");
		return 0;
	}

	std::printf("%zu of %zu entries are not what the manifest says\n", unmet, results.size());
	return 1;
}
