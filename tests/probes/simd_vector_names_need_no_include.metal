// EXPECT: valid
// DISASM: OpTypeVector %float 4
// DISASM: OpTypeVector %uint 2
// DISASM: OpTypeVector %int 3
//
// vector_* and simd_* vector names are Apple's without any include, as typedefs of the vector types.
#include <metal_stdlib>
using namespace metal;
kernel void simd_vector_names_need_no_include(device float* out [[buffer(0)]], constant vector_float4* a [[buffer(1)]], constant simd_uint2* b [[buffer(2)]], constant vector_int3* c [[buffer(3)]], uint i [[thread_position_in_grid]])
{ vector_half2 h = vector_half2(1.0); vector_char4 ch = vector_char4(1); vector_ushort2 us = vector_ushort2(2); out[i] = a[0].x + float(b[0].y) + float(c[0].z) + float(h.x) + float(ch.w) + float(us.x); }
