// EXPECT: valid
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ ArrayStride 4
// A packed_float3 buffer pointer parameter walks 12-byte elements: the pointee is the packed
// layout (an array of three floats at stride 4), not a 16-byte float3.
#include <metal_stdlib>
using namespace metal;
float sum3(device packed_float3* p, uint i) { return p[i][0] + p[i][1] + p[i][2]; }
kernel void k(device float* out [[buffer(0)]], device packed_float3* v [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = sum3(v, i); }
