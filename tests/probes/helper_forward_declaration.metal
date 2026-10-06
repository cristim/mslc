// EXPECT: valid
// DISASM: OpFunctionCall %float
#include <metal_stdlib>
using namespace metal;
float f(float x);
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
float f(float y) { return y * 3.0f; }
