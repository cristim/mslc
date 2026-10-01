#include "mslc/mslc.h"

#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "spirv.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <vector>

namespace {

	const char* const kVersion = "0.1.0";

	mslc::Stage stageFromApi(MslcStage stage) {
		switch (stage) {
			case MSLC_STAGE_VERTEX: return mslc::Stage::Vertex;
			case MSLC_STAGE_FRAGMENT: return mslc::Stage::Fragment;
			case MSLC_STAGE_KERNEL: return mslc::Stage::Kernel;
			case MSLC_STAGE_UNKNOWN: return mslc::Stage::None;
		}

		return mslc::Stage::None;
	}

	// Copies a string into a malloc'd buffer, since the C API hands ownership
	// to the caller and has no other way to say so.
	char* duplicate(const std::string& text) {
		auto* buffer = static_cast<char*>(std::malloc(text.size() + 1));
		if (!buffer) {
			return nullptr;
		}

		std::memcpy(buffer, text.c_str(), text.size() + 1);
		return buffer;
	}

}

extern "C" {

void mslc_default_options(MslcOptions* options) {
	if (!options) {
		return;
	}

	options->stage = MSLC_STAGE_UNKNOWN;
	options->localSizeX = 1;
	options->localSizeY = 1;
	options->localSizeZ = 1;
	options->imageSetPolicy = MSLC_SET_COMBINED;
}

int mslc_translate(const char* source, size_t sourceLength, const MslcOptions* options,
	uint8_t** outSpirv, size_t* outSpirvSize, char** outReflection, char** outError) {

	if (outSpirv) {
		*outSpirv = nullptr;
	}
	if (outSpirvSize) {
		*outSpirvSize = 0;
	}
	if (outReflection) {
		*outReflection = nullptr;
	}
	if (outError) {
		*outError = nullptr;
	}

	if (!source) {
		if (outError) {
			*outError = duplicate("no source given");
		}
		return 1;
	}

	MslcOptions effective;
	mslc_default_options(&effective);
	if (options) {
		effective = *options;
	}

	try {
		const std::string_view view(source, sourceLength);

		const std::vector<mslc::Token> tokens = mslc::tokenize(view);

		mslc::Parser parser(tokens);
		const mslc::TranslationUnit unit = parser.parse();

		const std::vector<const mslc::FunctionDecl*> entryPoints = mslc::selectEntryPoints(unit,
			stageFromApi(effective.stage));

		mslc::ModuleOptions moduleOptions;
		moduleOptions.localSizeX = effective.localSizeX ? effective.localSizeX : 1;
		moduleOptions.localSizeY = effective.localSizeY ? effective.localSizeY : 1;
		moduleOptions.localSizeZ = effective.localSizeZ ? effective.localSizeZ : 1;
		moduleOptions.separateImageSet = effective.imageSetPolicy == MSLC_SET_IMAGES;

		mslc::spirv::Builder builder;
		std::string reflection = mslc::emitModule(builder, unit, entryPoints, moduleOptions);

		std::vector<uint8_t> module;
		if (!builder.finalize(module)) {
			throw mslc::CompileError("emitted no entry point, so the module is not loadable");
		}

		// The reflection is a complete document already: the emitter built it
		// around the entry points it emitted, and there is more than one of them
		// whenever the source declared more than one.
		const std::string& document = reflection;

		auto* buffer = static_cast<uint8_t*>(std::malloc(module.size()));
		if (!buffer) {
			throw std::bad_alloc();
		}
		std::memcpy(buffer, module.data(), module.size());

		if (outSpirv) {
			*outSpirv = buffer;
		} else {
			std::free(buffer);
		}

		if (outSpirvSize) {
			*outSpirvSize = module.size();
		}

		if (outReflection) {
			*outReflection = duplicate(document);
		}

		return 0;
	} catch (const mslc::CompileError& error) {
		if (outError) {
			*outError = duplicate(error.what());
		}
		return 1;
	} catch (const std::exception& error) {
		if (outError) {
			*outError = duplicate(std::string("internal error: ") + error.what());
		}
		return 2;
	}
}

int mslc_validate(const uint8_t* spirv, size_t spirvSize, char** outError) {
	// Structural validation is done by spirv-val, which mslc does not link.
	// Reporting that honestly is better than returning success for a check
	// that did not run.
	(void)spirv;
	(void)spirvSize;

	if (outError) {
		*outError = duplicate("mslc was built without SPIRV-Tools, so mslc_validate cannot run; "
			"validate with spirv-val instead");
	}

	return 1;
}

void mslc_free(void* buffer) {
	std::free(buffer);
}

const char* mslc_version(void) {
	return kVersion;
}

}
