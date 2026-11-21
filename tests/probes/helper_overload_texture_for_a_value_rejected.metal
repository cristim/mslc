// EXPECT: error no matching function for call to "s"
// A texture is not a float: neither overload takes it.
#include <metal_stdlib>
using namespace metal;
float s(float t) { return 1.0f; }
float s(int t) { return 2.0f; }
kernel void k(texture2d<float> a [[texture(0)]], device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = s(a); }
