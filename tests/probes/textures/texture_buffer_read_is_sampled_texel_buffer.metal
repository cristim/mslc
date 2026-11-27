// EXPECT: valid
// A read-only texture_buffer is a sampled texel buffer: Dim Buffer, Sampled 1, SampledBuffer,
// fetched by one uint index with no Lod operand (Dim Buffer takes none), no sampler.
// DISASM: OpCapability SampledBuffer
// DISASM-NOT: OpCapability StorageImageReadWithoutFormat
// DISASM: OpTypeImage %float Buffer 2 0 0 1 Unknown
// DISASM-MATCH: OpImageFetch %v4float %[0-9]+ %[0-9]+
// DISASM-NO-MATCH: OpImageFetch %v4float %[0-9]+ %[0-9]+ 
// DISASM-NOT: OpTypeSampler
// DISASM: OpCapability ImageQuery
// DISASM-MATCH: OpImageQuerySize %uint %[0-9]+
// DISASM-NOT: OpImageQuerySizeLod
// REFLECT: { "kind": "TextureBuffer", "metal_index": 0, "descriptor": { "set": 0, "binding": 1 }, "descriptor_type": "UniformTexelBuffer", "texture_access": "Read"
// REFLECT-NOT: "kind": "Texture",
// Apple: air.read_texture_buffer_1d.v4f32 and air.get_width_texture_buffer_1d; the default access is read.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float> r [[texture(0)]], device float* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
  o[t] = r.read(t).x + float(r.get_width());
}
