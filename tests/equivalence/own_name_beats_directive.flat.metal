#include <metal_stdlib>
using namespace metal;
constant float cx = 3.0;
constant float x = 2.0;
float g() { return x; }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = g(); }
