// EXPECT: valid
// DISASM: OpCapability StorageImageReadWithoutFormat
// DISASM: OpCapability StorageImageWriteWithoutFormat
// DISASM-MATCH: OpImageRead %v4uint
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
uint4 load(texture2d<uint, access::read> src) { return src.read(uint2(0)); }
void store(texture2d<uint, access::write> dst) { dst.write(1u, uint2(0)); }
kernel void first(texture2d<uint, access::write> a, texture2d<uint, access::read> b) {
  store(a); a.write(load(b).x, uint2(1));
}
fragment float4 second(texture2d<uint, access::read> a, texture2d<uint, access::write> b) {
  b.write(load(a).y, uint2(1)); store(b);
  return float4(0.0);
}
