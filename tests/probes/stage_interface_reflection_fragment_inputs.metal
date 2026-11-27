// EXPECT: valid
// REFLECT: { "kind": "StageInput", "location": 0, "name": "TEXCOORD1", "type": "float2" }
// REFLECT: { "kind": "StageInput", "location": 1, "name": "tint", "type": "float4" }
// REFLECT-NOT: "kind": "StageOutput"
//
// A fragment function's [[stage_in]] struct lists the varyings it reads: the user name where a field has
// one and the field name where it has none, each with the Location the field has in this function. The
// fragment reads fewer varyings than the vertex function of the probe above writes, so these Locations
// are not that function's, and the names are what pairs them.
#include <metal_stdlib>
using namespace metal;
struct In {
	float2 uv [[user(TEXCOORD1)]];
	float4 tint;
};
fragment float4 stage_interface_fragment(In in [[stage_in]])
{ return in.tint * float4(in.uv, 0.0, 1.0); }
