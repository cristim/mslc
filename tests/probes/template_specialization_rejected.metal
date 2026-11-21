// EXPECT: error an explicit template specialization is not lowered
// Refs #341: refused, so the primary template is never used in its place.
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x) { return x; }
template <> float f<float>(float x) { return x + 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
