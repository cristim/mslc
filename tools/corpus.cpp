// Corpus runner: compiles every .metal file listed in a manifest and reports
// which reach structurally valid SPIR-V.
//
// The point is measurement. Progress on the MSL subset is "N of these files
// compile", not a claim, and a regression is visible immediately.

#include "mslc/mslc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

	struct Result {
		std::string path;
		bool compiled = false;
		bool valid = false;
		size_t spirvSize = 0;
		std::string diagnostic;
	};

	bool readFile(const std::string& path, std::string& out) {
		std::ifstream stream(path, std::ios::binary);
		if (!stream) {
			return false;
		}

		out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
		return true;
	}

	// A relative manifest entry names a file under root (the manifest's own
	// directory unless --root says otherwise), never one relative to whatever
	// directory the runner happens to be started from.
	std::string resolve(const std::string& root, const std::string& entry) {
		if (entry.empty() || entry[0] == '/' || root.empty()) {
			return entry;
		}

		return root + "/" + entry;
	}

	// Single-quotes an argument for /bin/sh, so paths with spaces survive
	// popen() and system().
	std::string shellQuote(const std::string& arg) {
		std::string quoted = "'";
		for (const char c : arg) {
			if (c == '\'') {
				quoted += "'\\''";
			} else {
				quoted += c;
			}
		}

		return quoted + "'";
	}

	bool validateWithSpirvVal(const std::string& spirvVal, const std::string& spvPath, std::string& detail) {
		const std::string command = shellQuote(spirvVal) + " --target-env vulkan1.3 " + shellQuote(spvPath)
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

}

int main(int argc, char** argv) {
	if (argc < 2) {
		std::fprintf(stderr, "usage: mslc-corpus <manifest> [--root <dir>] [--spirv-val <path>] [--keep-output <dir>]\n");
		return 2;
	}

	const std::string manifestPath = argv[1];

	// Default into TMPDIR rather than a fixed /tmp path, so the scratch output
	// follows the caller's idea of where temporary files belong.
	const char* tmpdir = std::getenv("TMPDIR");
	std::string outputDirectory = std::string(tmpdir && *tmpdir ? tmpdir : "/tmp") + "/mslc-corpus";

	const size_t manifestSlash = manifestPath.find_last_of('/');
	std::string root = manifestSlash == std::string::npos ? "" : manifestPath.substr(0, manifestSlash);
	std::string spirvVal = "spirv-val";

	for (int i = 2; i + 1 < argc; ++i) {
		if (std::strcmp(argv[i], "--keep-output") == 0) {
			outputDirectory = argv[++i];
		} else if (std::strcmp(argv[i], "--root") == 0) {
			root = argv[++i];
		} else if (std::strcmp(argv[i], "--spirv-val") == 0) {
			spirvVal = argv[++i];
		}
	}

	// A missing validator must fail the run rather than let "not checked"
	// read as either a compiler regression or a pass.
	if (std::system((shellQuote(spirvVal) + " --version >/dev/null 2>&1").c_str()) != 0) {
		std::fprintf(stderr, "mslc-corpus: cannot run \"%s\"; install SPIRV-Tools\n", spirvVal.c_str());
		return 2;
	}

	// Without this a missing directory turns every shader into "INVALID", which
	// reads like a compiler regression rather than a missing scratch dir.
	std::error_code ec;
	std::filesystem::create_directories(outputDirectory, ec);
	if (ec) {
		std::fprintf(stderr, "mslc-corpus: cannot create %s: %s\n", outputDirectory.c_str(), ec.message().c_str());
		return 2;
	}

	std::ifstream manifest(manifestPath);
	if (!manifest) {
		std::fprintf(stderr, "mslc-corpus: cannot read %s\n", manifestPath.c_str());
		return 2;
	}

	std::vector<Result> results;
	std::string line;
	while (std::getline(manifest, line)) {
		if (line.empty() || line[0] == '#') {
			continue;
		}

		Result result;
		result.path = resolve(root, line);

		std::string source;
		if (!readFile(result.path, source)) {
			result.diagnostic = "cannot read file";
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
			results.push_back(result);
			continue;
		}

		result.compiled = true;
		result.spirvSize = spirvSize;

		// Write the module out so spirv-val can check it, which is a stronger
		// statement than "the compiler returned success".
		const size_t slash = line.find_last_of('/');
		const std::string name = slash == std::string::npos ? line : line.substr(slash + 1);
		const std::string spvPath = outputDirectory + "/" + name + ".spv";

		FILE* out = std::fopen(spvPath.c_str(), "wb");
		if (out) {
			std::fwrite(spirv, 1, spirvSize, out);
			std::fclose(out);

			std::string detail;
			result.valid = validateWithSpirvVal(spirvVal, spvPath, detail);
			if (!result.valid) {
				result.diagnostic = detail;
			}
		} else {
			result.diagnostic = "cannot write " + spvPath;
		}

		mslc_free(spirv);
		results.push_back(result);
	}

	size_t compiled = 0;
	size_t valid = 0;

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
	}

	std::printf("\n%zu of %zu compile, %zu of %zu reach spirv-val-valid SPIR-V\n",
		compiled, results.size(), valid, results.size());

	// An empty manifest would otherwise pass as "0 of 0 valid".
	if (results.empty()) {
		std::fprintf(stderr, "mslc-corpus: %s lists no shaders\n", manifestPath.c_str());
		return 1;
	}

	return valid == results.size() ? 0 : 1;
}
