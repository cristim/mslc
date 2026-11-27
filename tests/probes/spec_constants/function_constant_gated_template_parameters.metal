// EXPECT: valid
//
// The parameters of a function template's instance may be gated as well: its parser sees the file-scope constant.
#include <metal_stdlib>
using namespace metal;
constant bool isArray [[function_constant(0)]];
template <typename T>
static inline T pick(texture2d<T> flat [[texture(0), function_constant(isArray)]],
                     texture2d_array<T> layered [[texture(0), function_constant(isArray)]]) {
	return isArray ? layered.read(uint2(0), 0).x : flat.read(uint2(0)).x;
}
kernel void k(device float* out [[buffer(0)]], texture2d<float> flat [[texture(0), function_constant(isArray)]],
              texture2d_array<float> layered [[texture(0), function_constant(isArray)]]) {
	out[0] = pick(flat, layered);
}
