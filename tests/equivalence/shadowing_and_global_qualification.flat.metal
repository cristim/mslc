#include <metal_stdlib>
using namespace metal;
constant float k = 1.0;
float twice(float x) { return x + x; }
constant float nk = 5.0;
float ntwice(float x) { return x * 2.5; }
float both(float x) { float v = x; return twice(v) + ntwice(v) + nk + k; }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = both(float(i)) + k + twice(1.0); }
