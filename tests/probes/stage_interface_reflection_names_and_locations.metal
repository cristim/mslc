// EXPECT: valid
// REFLECT: { "kind": "StageOutput", "location": 0, "name": "COLOR0", "type": "float4" }
// REFLECT: { "kind": "StageOutput", "location": 1, "name": "TEXCOORD0", "type": "float" }
// REFLECT: { "kind": "StageOutput", "location": 2, "name": "TEXCOORD1", "type": "float2" }
// REFLECT-NOT: "name": "position"
//
// A vertex function's returned struct names each varying and its Location in the reflection, apart from the
// position, so a host that pairs this function with a fragment function compiled separately can line up the
// Locations by name. The user name is used where a field has one, and the field name stays out.
#include <metal_stdlib>
using namespace metal;
struct Out {
	float4 position [[position]];
	float4 color [[user(COLOR0)]];
	float extra [[user(TEXCOORD0)]];
	float2 uv [[user(TEXCOORD1)]];
};
vertex Out stage_interface_vertex(uint vid [[vertex_id]])
{ Out o; o.position = float4(float(vid)); o.color = float4(1.0); o.extra = 2.0; o.uv = float2(3.0); return o; }
