// EXPECT: valid
// A float access::write texture2d is a storage image with a float sampled type; write takes a float4, or a number put in every channel, at a uint2 or ushort2 coordinate.
// DISASM-MATCH: OpTypeImage %float 2D 0 0 0 2 Unknown
// DISASM-NOT: OpTypeImage %uint
// DISASM: NonReadable
// DISASM-NOT: NonWritable
// DISASM: OpCapability StorageImageWriteWithoutFormat
// DISASM-NOT: StorageImageReadWithoutFormat
// DISASM: OpImageWrite
// DISASM-NOT: OpFConvert
// REFLECT: "metal_index": 2, "descriptor": { "set": 0, "binding": 0 }, "texture_access": "Write"
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::write> dst [[texture(2)]], uint3 g [[thread_position_in_grid]]) {
  dst.write(float4(float(g.x), 1.0, 2.0, 3.0), g.xy);
  dst.write(float4(0.5), ushort2(1, 2));
  dst.write(3.5f, uint2(3, 3));
  dst.write(-3, g.xy);
}
