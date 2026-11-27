// EXPECT: valid
// QUIRK (helper parameter attributes): a parameter of a function that is not an entry point may carry an
// entry point's attributes, which have no effect there, so indices may repeat and need not match the
// caller's: the arguments are what the calls pass. Apple's compiler accepts this (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
struct In { float a; float b; };
template <typename T> T pick(T x, texture2d<float> t [[texture(0)]], texture2d_array<float> ta [[texture(0)]],
	sampler s [[sampler(3)]], constant float& c [[buffer(1)]]) {
	return x + t.read(uint2(0)).x * 0.0f + ta.read(uint2(0), 0).x * 0.0f + c;
}
float first(device float* d [[buffer(1)]], uint i [[thread_position_in_grid]]) { return d[i] * 0.0f; }
float sum(In v [[stage_in]], uint i [[thread_position_in_grid]], float u [[user(locn0)]], uint m [[sample_mask]],
	uint p [[primitive_id]]) { return v.a + v.b + float(i); }
kernel void k(device float* out [[buffer(0)]], texture2d<float> t [[texture(4)]], texture2d_array<float> ta [[texture(5)]],
	sampler s [[sampler(0)]], constant float& c [[buffer(2)]], uint i [[thread_position_in_grid]])
{ In v; v.a = 1.0f; v.b = 2.0f; out[i] = sum(v, i * 10u, 0.0f, 0u, 0u) + pick(100.0f, t, ta, s, c) - c + first(out, i); }
