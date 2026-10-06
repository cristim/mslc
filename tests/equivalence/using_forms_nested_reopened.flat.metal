#include <metal_stdlib>
using namespace metal;
constant float a = 1.0;
constant float b = 2.0;
float f(float x) { return x + a + b; }
float g(float x) { return f(x) * a; }
float h(float x) { return g(x) + b; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = g(float(i)) + h(float(i)) + f(1.0); }
