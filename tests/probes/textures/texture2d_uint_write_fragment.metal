// EXPECT: valid
// DISASM: OpImageWrite
// DISASM: OpUMod
// DISASM: OpUDiv
// REFLECT: "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Write"
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[position]]; };
fragment float4 hit(texture2d<uint, access::write> dst [[texture(9)]], In in [[stage_in]]) {
  uint i = uint(in.p.x) + uint(in.p.y) * 3u;
  uint width = dst.get_width();
  dst.write(i == 0u ? 0u : i == 1u ? 1u : i == 2u ? 16777217u : i == 3u ? 0x80000001u : 0xffffffffu, uint2(i % width, i / width));
  return float4(0.0);
}
