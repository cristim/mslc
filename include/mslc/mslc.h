/*
 * mslc - a Metal Shading Language compiler.
 *
 * Translates a subset of MSL to Vulkan SPIR-V. The public surface is a C ABI
 * on purpose: the implementation is C++ today, but callers only ever see this
 * header, so the core can be rewritten in another language without touching
 * indium or darling-metal.
 *
 * All functions are thread-safe unless noted. Returned buffers are owned by
 * the caller and must be released with mslc_free().
 */

#ifndef MSLC_MSLC_H
#define MSLC_MSLC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	MSLC_STAGE_UNKNOWN = 0,
	MSLC_STAGE_VERTEX = 1,
	MSLC_STAGE_FRAGMENT = 2,
	MSLC_STAGE_KERNEL = 3,
} MslcStage;

/* Descriptor set assignment. Not consulted yet: a vertex or kernel function's
 * bindings are in set 0 and a fragment function's in set 1 under either value. */
typedef enum {
	MSLC_SET_COMBINED = 0,
	MSLC_SET_IMAGES = 1,
} MslcImageSetPolicy;

/* Options are passed by pointer and new fields are appended, as sourcePath,
 * includeDirs and includeDirCount were. A caller must be compiled against the
 * header it links with and start from mslc_default_options(); a caller built
 * against an older header passes a shorter struct, which mslc would read past. */
typedef struct {
	/* Overrides the stage when the caller knows it. MSLC_STAGE_UNKNOWN asks
	 * mslc to infer it from the source, which works when the file declares a
	 * single entry point of one stage. */
	MslcStage stage;

	/* Workgroup size for a kernel entry point. Zero components fall back to
	 * the MSL default of 1, matching thread_position_in_grid reaching one
	 * invocation per thread. */
	uint32_t localSizeX;
	uint32_t localSizeY;
	uint32_t localSizeZ;

	MslcImageSetPolicy imageSetPolicy;

	/* Path of the source passed to mslc_translate, or NULL when it did not come
	 * from a file. A quoted #include is looked for first in this file's
	 * directory, so a source with no path can include only from includeDirs. The
	 * file is not read; the path is used for resolving includes and for naming
	 * the source in diagnostics. */
	const char* sourcePath;

	/* Directories searched, in order, for a quoted #include after the source's
	 * own directory. An include may resolve only inside the source's directory
	 * and these; an absolute path, or one that escapes them, is an error. An
	 * entry must name an existing directory; an empty string is refused rather
	 * than read as the current directory. */
	const char* const* includeDirs;
	size_t includeDirCount;

	/* Macros defined before the source is read, as if each were given as -D name=value on a
	 * compiler's command line. macroNames and macroValues hold macroCount entries each; a NULL
	 * macroValues, or a NULL entry in it, defines the macro as 1, and an empty value defines it with
	 * an empty expansion. A name must be an identifier and a value may not contain a line break,
	 * otherwise the translation fails with a diagnostic. The source may define a macro of the same
	 * name again, which replaces the value from here (it does not fail as defining a macro twice in
	 * the source does), or #undef it. A name mslc provides as a builtin cannot be defined. */
	const char* const* macroNames;
	const char* const* macroValues;
	size_t macroCount;

	/* Non-zero makes every kernel without a threadgroup or simdgroup barrier (in its body or in any
	 * function of the source) skip the threads whose position in the grid is outside the grid size that
	 * the specialization constants at the ids named by the reflection's "grid_size_spec_ids" give. The
	 * constants default to 0xFFFFFFFF, so a dispatch that does not set them runs every thread, and a
	 * dispatch of a grid that is not a multiple of the threadgroup size sets them to the grid size
	 * (Metal's dispatchThreads:). A kernel with a barrier has no such ids in its reflection: a thread that
	 * returns before a barrier leaves the others waiting for it in Vulkan. */
	uint32_t maskGridSize;
} MslcOptions;

/* Fills options with the defaults: stage inferred, local size 1/1/1, combined
 * descriptor set, no source path, no include directories and no predefined macros. */
void mslc_default_options(MslcOptions* options);

/* Translates MSL source to a SPIR-V module.
 *
 * On success returns 0 and writes:
 *   outSpirv        - module bytes, owned by the caller, mslc_free()
 *   outSpirvSize    - length of outSpirv in bytes
 *   outReflection   - optional; when non-NULL, receives a NUL-terminated JSON
 *                     document describing the entry point, descriptor bindings
 *                     and local size. Owned by the caller, mslc_free().
 *
 * On failure returns a non-zero value, leaves outSpirv NULL, and when outError
 * is non-NULL stores a NUL-terminated diagnostic owned by the caller.
 *
 * Unsupported input is always a hard failure with a diagnostic. mslc never
 * emits a module it could not fully translate, so a caller that gets a
 * non-zero return can report the problem rather than run something wrong. */
int mslc_translate(const char* source, size_t sourceLength, const MslcOptions* options,
	uint8_t** outSpirv, size_t* outSpirvSize, char** outReflection, char** outError);

/* Compiles a genuinely empty active translation unit to a Universal SPIR-V 1.5
 * Linkage library, with no entry points or functions. Only comments, preprocessing,
 * semicolons and using namespace metal; are supported. This is not a Vulkan
 * executable. stage must be UNKNOWN; local size and image set policy are unused.
 * Source path and include options have the same meaning as in mslc_translate.
 * Returns 0 on success, 1 for unsupported input, 2 for internal/allocation failure.
 * Provided outputs are reset before processing. Successful bytes use mslc_free;
 * a requested diagnostic also uses mslc_free, but may be NULL on allocation failure.
 * No C++ exception escapes this function. */
int mslc_compile_library(const char* source, size_t sourceLength, const MslcOptions* options,
	uint8_t** outSpirv, size_t* outSpirvSize, char** outError);

/* Validates a module with SPIRV-Tools if mslc was built with that support.
 * Returns 0 when the module is structurally valid. */
int mslc_validate(const uint8_t* spirv, size_t spirvSize, char** outError);

/* Releases any buffer handed out by this API. Safe on NULL. */
void mslc_free(void* buffer);

/* Library version, for callers that want to record what produced a module. */
const char* mslc_version(void);

#ifdef __cplusplus
}
#endif

#endif
