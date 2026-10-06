// EXPECT: error undeclared type "Nope"
#include <metal_stdlib>
using namespace metal;
float f(Nope x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
