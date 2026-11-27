// EXPECT: valid
//
// A helper's parameters may carry the same gates as the entry point's, which have no effect there.
#include <metal_stdlib>
using namespace metal;
constant bool isArray [[function_constant(0)]];
static inline float pick(texture2d<float> flat [[function_constant(isArray)]],
                         texture2d_array<float> layered [[texture(0), function_constant(isArray)]]) {
	return isArray ? layered.read(uint2(0), 0).x : flat.read(uint2(0)).x;
}
kernel void k(device float* out [[buffer(0)]], texture2d<float> flat [[texture(0), function_constant(isArray)]],
              texture2d_array<float> layered [[texture(0), function_constant(isArray)]]) {
	out[0] = pick(flat, layered);
}
