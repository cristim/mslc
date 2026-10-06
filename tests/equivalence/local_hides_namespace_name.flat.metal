#include <metal_stdlib>
using namespace metal;
constant float nk = 100.0;
float f(float k) { float limit = k + 1.0; return limit; }
float g(float x) { float k = x * 2.0; return k + nk; }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(float(i)) + g(float(i)) + nk; }
