// EXPECT: valid
// write and read_write texture_buffers are storage texel buffers: Dim Buffer, Sampled 2, ImageBuffer,
// format-less like the other storage textures, a scalar uint coordinate.
// DISASM: OpCapability ImageBuffer
// DISASM: OpCapability StorageImageReadWithoutFormat
// DISASM: OpCapability StorageImageWriteWithoutFormat
// DISASM: OpTypeImage %float Buffer 0 0 0 2 Unknown
// DISASM: OpTypeImage %int Buffer 0 0 0 2 Unknown
// DISASM: OpTypeImage %uint Buffer 0 0 0 2 Unknown
// DISASM: OpTypeImage %float Buffer 2 0 0 1 Unknown
// DISASM: OpCapability SampledBuffer
// DISASM: NonReadable
// DISASM-MATCH: OpImageRead %v4float %[0-9]+ %[0-9]+
// DISASM-NO-MATCH: OpImageRead %v4float %[0-9]+ %[0-9]+ 
// DISASM-MATCH: OpImageWrite %[0-9]+ %[0-9]+ %[0-9]+
// DISASM: OpImageQuerySize %uint
// DISASM-NOT: OpImageQuerySizeLod
// REFLECT: "descriptor_type": "StorageTexelBuffer", "texture_access": "ReadWrite"
// REFLECT: "descriptor_type": "StorageTexelBuffer", "texture_access": "Write"
// REFLECT: "descriptor_type": "UniformTexelBuffer", "texture_access": "Read"
// Apple: texture_buffer<half|short|ushort, ...> read and write by uint or ushort index.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read_write> a [[texture(0)]],
              texture_buffer<half, access::write> w [[texture(1)]],
              texture_buffer<int, access::read_write> i [[texture(2)]],
              texture_buffer<ushort, access::read_write> u [[texture(3)]],
              texture_buffer<float, access::read> r [[texture(4)]],
              uint t [[thread_position_in_grid]]) {
  a.write(a.read(t) + r.read(t), t);
  w.write(half4(1), ushort(t));
  w.write(float(a.get_width()), t);
  i.write(i.read(t) + int4(1), t);
  u.write(u.read(ushort(t)), t);
}
