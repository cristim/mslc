#include <metal_stdlib>
using namespace metal;
constant float v = 4.0;
float g() { return v; }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = g(); }
