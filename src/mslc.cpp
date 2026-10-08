#include "mslc/mslc.h"

#include "lexer.h"
#include "parser.h"
#include "preprocessor.h"
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

	enum class ArtifactKind { VulkanExecutable, EmptyLibrary };

	void compile(const char* source, size_t sourceLength, const MslcOptions& effective,
		ArtifactKind kind, std::vector<uint8_t>& module, std::string& reflection) {
		mslc::PreprocessOptions preprocessOptions;
		if (effective.sourcePath) {
			preprocessOptions.sourcePath = effective.sourcePath;
		}
		if (effective.includeDirCount > 0 && !effective.includeDirs) {
			throw mslc::CompileError("includeDirCount is nonzero but includeDirs is NULL");
		}
		for (size_t i = 0; i < effective.includeDirCount; ++i) {
			if (!effective.includeDirs[i]) {
				throw mslc::CompileError("includeDirs has a NULL entry");
			}
			preprocessOptions.includeDirs.emplace_back(effective.includeDirs[i]);
		}

		const mslc::PreprocessedSource preprocessed = mslc::preprocess(
			std::string_view(source, sourceLength), preprocessOptions);
		mslc::Parser parser(preprocessed.tokens);
		const mslc::TranslationUnit unit = [&]() {
			try {
				return parser.parse();
			} catch (const mslc::CompileError& error) {
				throw mslc::CompileError(mslc::describeOrigin(preprocessed, parser.position()) + ": "
					+ error.what());
			}
		}();

		mslc::spirv::Builder builder;
		if (kind == ArtifactKind::EmptyLibrary) {
			if (effective.stage != MSLC_STAGE_UNKNOWN) {
				throw mslc::CompileError("library compilation requires stage UNKNOWN");
			}
			const auto& tokens = preprocessed.tokens;
			for (size_t i = 0; i < tokens.size();) {
				if (tokens[i].kind == mslc::TokenKind::EndOfFile
					|| tokens[i].kind == mslc::TokenKind::Semicolon) {
					++i;
					continue;
				}
				if (tokens.size() - i >= 4
					&& tokens[i].kind == mslc::TokenKind::Identifier && tokens[i].text == "using"
					&& tokens[i + 1].kind == mslc::TokenKind::Identifier && tokens[i + 1].text == "namespace"
					&& tokens[i + 2].kind == mslc::TokenKind::Identifier && tokens[i + 2].text == "metal"
					&& tokens[i + 3].kind == mslc::TokenKind::Semicolon) {
					i += 4;
					continue;
				}
				throw mslc::CompileError(mslc::describeOrigin(preprocessed, i)
					+ ": library compilation supports only empty translation units");
			}
			using namespace mslc::spirv;
			builder.emit(OpCapability, {static_cast<uint32_t>(Capability::Shader)});
			builder.emit(OpCapability, {static_cast<uint32_t>(Capability::Linkage)});
			builder.emit(OpCapability, {static_cast<uint32_t>(Capability::PhysicalStorageBufferAddresses)});
			builder.setSection(Section::MemoryModel);
			builder.emit(OpMemoryModel, {static_cast<uint32_t>(AddressingModel::PhysicalStorageBuffer64),
				static_cast<uint32_t>(MemoryModel::GLSL450)});
			builder.finalizeLibrary(module);
			return;
		}

		const auto entryPoints = mslc::selectEntryPoints(unit, stageFromApi(effective.stage));
		mslc::ModuleOptions moduleOptions;
		moduleOptions.localSizeX = effective.localSizeX ? effective.localSizeX : 1;
		moduleOptions.localSizeY = effective.localSizeY ? effective.localSizeY : 1;
		moduleOptions.localSizeZ = effective.localSizeZ ? effective.localSizeZ : 1;
		moduleOptions.separateImageSet = effective.imageSetPolicy == MSLC_SET_IMAGES;
		reflection = mslc::emitModule(builder, unit, entryPoints, moduleOptions);
		if (!builder.finalize(module)) {
			throw mslc::CompileError("emitted no entry point, so the module is not loadable");
		}
	}

#ifdef MSLC_LIBRARY_ALLOCATION_TEST
	thread_local int libraryFailAllocation = 0;
#endif
	void* libraryAllocate(size_t size, int site) noexcept {
#ifdef MSLC_LIBRARY_ALLOCATION_TEST
		if (libraryFailAllocation == site) {
			return nullptr;
		}
#else
		(void)site;
#endif
		return std::malloc(size);
	}

	int libraryError(const char* text, int status, char** outError) noexcept {
		if (outError) {
			const size_t size = std::strlen(text) + 1;
			*outError = static_cast<char*>(libraryAllocate(size, 2));
			if (!*outError) {
				return 2;
			}
			std::memcpy(*outError, text, size);
		}
		return status;
	}

}

extern "C" {

#ifdef MSLC_LIBRARY_ALLOCATION_TEST
void mslc_test_library_fail_allocation(int site) {
	libraryFailAllocation = site;
}
#endif

int mslc_compile_library(const char* source, size_t sourceLength, const MslcOptions* options,
	uint8_t** outSpirv, size_t* outSpirvSize, char** outError) {
	if (outSpirv) *outSpirv = nullptr;
	if (outSpirvSize) *outSpirvSize = 0;
	if (outError) *outError = nullptr;
	try {
		if (!source) return libraryError("no source given", 1, outError);
		MslcOptions effective;
		mslc_default_options(&effective);
		if (options) effective = *options;
		std::vector<uint8_t> module;
		std::string reflection;
		compile(source, sourceLength, effective, ArtifactKind::EmptyLibrary, module, reflection);
		auto* buffer = static_cast<uint8_t*>(libraryAllocate(module.size(), 1));
		if (!buffer) return libraryError("internal error: allocation failure", 2, outError);
		std::memcpy(buffer, module.data(), module.size());
		if (outSpirv) *outSpirv = buffer;
		else std::free(buffer);
		if (outSpirvSize) *outSpirvSize = module.size();
		return 0;
	} catch (const mslc::CompileError& error) {
		return libraryError(error.what(), 1, outError);
	} catch (const std::exception& error) {
		return libraryError(error.what(), 2, outError);
	} catch (...) {
		return libraryError("internal error: unknown exception", 2, outError);
	}
}

void mslc_default_options(MslcOptions* options) {
	if (!options) {
		return;
	}

	options->stage = MSLC_STAGE_UNKNOWN;
	options->localSizeX = 1;
	options->localSizeY = 1;
	options->localSizeZ = 1;
	options->imageSetPolicy = MSLC_SET_COMBINED;
	options->sourcePath = nullptr;
	options->includeDirs = nullptr;
	options->includeDirCount = 0;
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
		std::vector<uint8_t> module;
		std::string reflection;
		compile(source, sourceLength, effective, ArtifactKind::VulkanExecutable, module, reflection);

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
