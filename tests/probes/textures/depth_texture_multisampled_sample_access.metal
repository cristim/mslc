// EXPECT: valid
// depth2d_ms and depth2d_ms_array accept both access::read (the default) and access::sample.
// DISASM-MATCH: OpTypeImage %float 2D 1 0 1 1 Unknown
// DISASM-MATCH: OpTypeImage %float 2D 1 1 1 1 Unknown
// DISASM-MATCH: OpImageFetch %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Sample %[0-9A-Za-z_]+
// DISASM: OpImageQuerySamples %uint
// REFLECT: "texture_access": "Sample"
// REFLECT: "texture_access": "Read"
#include <metal_stdlib>
using namespace metal;
float readSample(depth2d_ms<float, access::sample> t, uint2 p) { return t.read(p, 0); }
float readDefault(depth2d_ms<float> t, uint2 p) { return t.read(p, 1); }
kernel void hit(depth2d_ms<float, access::sample> a [[texture(0)]],
    depth2d_ms_array<float, access::sample> b [[texture(1)]],
    depth2d_ms<float, access::read> c [[texture(2)]],
    device float4* o [[buffer(0)]]) {
  o[0] = float4(readSample(a, uint2(0)), readDefault(c, uint2(0)), b.read(uint2(1), 0u, 1),
    float(a.get_num_samples() + b.get_array_size() + c.get_width() + b.get_height()));
}
