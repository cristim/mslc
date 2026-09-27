// Corpus runner: compiles every .metal file listed in a manifest and reports
// which reach structurally valid SPIR-V.
//
// The point is measurement. Progress on the MSL subset is "N of these files
// compile", not a claim, and a regression is visible immediately.

#include "mslc/mslc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

}

int main(int argc, char** argv) {
	if (argc < 2) {
		std::fprintf(stderr, "usage: mslc-corpus <manifest> [--keep-output <dir>]\n");
		return 2;
	}

	const std::string manifestPath = argv[1];
	std::string outputDirectory = "/tmp/mslc-corpus";

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
	while (std::getline(manifest, line)) {
		if (line.empty() || line[0] == '#') {
			continue;
		}

		Result result;
		result.path = resolve(manifestPath, line);

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
			result.valid = validateWithSpirvVal(spvPath, detail);
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

	return valid == results.size() ? 0 : 1;
}
