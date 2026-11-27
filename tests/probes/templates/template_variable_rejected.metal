// EXPECT: error a template declaration that is not a function is not lowered
// Refs #341: a variable template initialised with a call is not mistaken for a function.
#include <metal_stdlib>
using namespace metal;
template <typename T> constant T one = T(1);
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
