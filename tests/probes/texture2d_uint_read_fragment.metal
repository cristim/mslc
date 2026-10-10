// EXPECT: valid
// DISASM-MATCH: OpImageRead %v4uint
// REFLECT: "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Read"
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<uint, access::read> src [[texture(9)]]) {
  return float4(src.read(uint2(1, 2)));
}
