// EXPECT: valid
// A vertex function's resources are in set 0, a fragment function's in set 1, in one
// module that holds both.
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 0, "binding": 0 }, "texture_access": "Sample", "param_index": 1, "name": "vt" }
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 1, "name": "ft" }
#include <metal_stdlib>
using namespace metal;
struct V { float4 position [[position]]; float2 uv; };
vertex V v(uint i [[vertex_id]], texture2d<float> vt [[texture(0)]])
{ V o; o.position = vt.read(uint2(i, 0)); o.uv = float2(0.0); return o; }
fragment float4 f(V in [[stage_in]], texture2d<float> ft [[texture(0)]])
{ return ft.read(uint2(0)); }
