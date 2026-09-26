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

/* Descriptor set assignment. MSLC_SET_IMAGES lets image bindings occupy their
 * own set, which indium expects: it mirrors Metal's split between a vertex
 * stage's buffer bindings and a fragment stage's, which land in different
 * sets. */
typedef enum {
	MSLC_SET_COMBINED = 0,
	MSLC_SET_IMAGES = 1,
} MslcImageSetPolicy;

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
} MslcOptions;

/* Fills options with the defaults: stage inferred, local size 1/1/1, combined
 * descriptor set. */
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
