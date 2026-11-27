// EXPECT: valid
// The arguments reach the callee in the order written.
// DISASM-MATCH: OpFunctionCall %float %[0-9a-z_]+ %float_2 %float_5
#include <metal_stdlib>
using namespace metal;
float sub(float a, float b) { return a - b; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = sub(2.0f, 5.0f); }
