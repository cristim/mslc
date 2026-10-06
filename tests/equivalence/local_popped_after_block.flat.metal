#include <metal_stdlib>
using namespace metal;
constant float x = 1.0;
constant float ax = 2.0;
float h() { float r = 0.0; { float x = 5.0; r = x; } return r + ax; }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = h() + x; }
