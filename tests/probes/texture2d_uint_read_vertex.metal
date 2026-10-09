// EXPECT: valid
// DISASM-MATCH: OpImageRead %v4uint
// REFLECT: "descriptor": { "set": 0, "binding": 0 }, "texture_access": "Read"
#include <metal_stdlib>
using namespace metal;
struct V { float4 p [[position]]; };
vertex V hit(texture2d<uint, access::read> src [[texture(1)]]) { V o; o.p = float4(src.read(uint2(0))); return o; }
