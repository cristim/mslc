// EXPECT: valid
// REFLECT: "descriptor": { "set": 0, "binding": 0 }, "texture_access": "Sample"
// REFLECT: "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Write"
#include <metal_stdlib>
using namespace metal;
struct V { float4 p [[position]]; };
vertex V vert(texture2d<float> src [[texture(3)]]) { V o; o.p = src.read(uint2(0)); return o; }
fragment float4 frag(texture2d<uint, access::write> dst [[texture(3)]]) { dst.write(1u, uint2(0)); return float4(0.0); }
