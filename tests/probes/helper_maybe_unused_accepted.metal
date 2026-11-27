// EXPECT: valid
// DISASM: OpFunctionCall %float
#include <metal_stdlib>
using namespace metal;

#define FERAL_ALWAYS_INLINE [[maybe_unused]] inline __attribute__((always_inline))

FERAL_ALWAYS_INLINE float scale_used(float x) { return x * 2.0f; }
FERAL_ALWAYS_INLINE float scale_unused(float x) { return x * 4.0f; }
[[]] float empty_attr(float x) { return x + 1.0f; }
[[maybe_unused, maybe_unused]] float repeated_attr(float x) { return x + 3.0f; }

namespace {
FERAL_ALWAYS_INLINE float anon_scale(float x) { return x * 0.5f; }
}

kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[i] = anon_scale(repeated_attr(empty_attr(scale_used(float(i)))));
}
