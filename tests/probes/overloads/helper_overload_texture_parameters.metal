// EXPECT: valid
// A texture parameter takes a texture of exactly its kind, so two overloads of one arity
// differ by it.
#include <metal_stdlib>
using namespace metal;
float s(texture2d<float> t) { return 1.0f; }
float s(texturecube<float> t) { return 2.0f; }
kernel void k(texture2d<float> a [[texture(0)]], texturecube<float> b [[texture(1)]],
	device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = s(a) + s(b); }
