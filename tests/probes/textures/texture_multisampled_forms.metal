// EXPECT: valid
// A multisampled texture is a sampled image with MS=1 (a sampled one needs no ImageMSArray, which is for storage images): read takes a coordinate, the array index where there is one, and a sample index (OpImageFetch with the Sample operand), get_num_samples is OpImageQuerySamples, the size queries use OpImageQuerySize.
// DISASM-NOT: OpCapability ImageMSArray
// DISASM-MATCH: OpTypeImage %float 2D 2 0 1 1 Unknown
// DISASM-MATCH: OpTypeImage %float 2D 2 1 1 1 Unknown
// DISASM-MATCH: OpTypeImage %float 2D 1 0 1 1 Unknown
// DISASM-MATCH: OpTypeImage %uint 2D 2 0 1 1 Unknown
// DISASM-MATCH: OpImageFetch %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Sample %[0-9A-Za-z_]+
// DISASM: OpImageQuerySamples %uint
// DISASM-NOT: OpImageQuerySizeLod
// DISASM-MATCH: OpImageQuerySize %v3uint
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d_ms<float> a [[texture(0)]], texture2d_ms_array<float, access::read> b [[texture(1)]], depth2d_ms<float> d [[texture(2)]], texture2d_ms<uint> u [[texture(3)]],
    device float4* o [[buffer(0)]], uint2 t [[thread_position_in_grid]]) {
  o[0] = a.read(t, 1) + b.read(t, 2u, 3) + float4(d.read(t, 0)) + float4(u.read(t, 1));
  o[1] = float4(a.get_num_samples(), a.get_width(), a.get_height(), b.get_array_size() + b.get_num_samples());
}
