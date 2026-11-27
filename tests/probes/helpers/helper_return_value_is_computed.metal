// EXPECT: valid
// The value the body computes is the one returned, not a constant.
// DISASM-MATCH: OpFAdd %float %[0-9a-z_]+ %float_1
// DISASM-NO-MATCH: OpReturnValue %float_
#include <metal_stdlib>
using namespace metal;
float add_one(float x) { return x + 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = add_one(2.0f); }
