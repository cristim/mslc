// EXPECT: error operator | needs integer operands, and a floating-point scalar is beside a bool vector
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { bool2 a = bool2((i&1)!=0, (i&2)!=0); auto r = a | 0.5f; out[i] = float(r.x)+2*float(r.y); }
