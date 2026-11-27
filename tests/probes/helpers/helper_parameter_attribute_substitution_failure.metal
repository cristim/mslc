// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// QUIRK (helper parameter attributes): an attribute that does not suit a template's instance only removes the
// template from the candidates, so the function beside it is called: out[i] is 2.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x [[texture(0)]]) { return 1.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(1.0f); }
