// EXPECT: valid
// REFLECT: { "name": "scale", "type": "float", "index": 2, "spec_id": 65538, "defined_spec_id": 131074 }
// REFLECT: { "name": "tint", "type": "bool", "index": 3, "spec_id": 65539, "defined_spec_id": 131075 }
//
// Function constants in a vertex and a fragment function of one module: each entry point lists the
// one it reads.
#include <metal_stdlib>
using namespace metal;
constant float scale [[function_constant(2)]];
constant bool tint [[function_constant(3)]];
struct V { float4 p [[position]]; };
vertex V vs(uint id [[vertex_id]]) { V v; v.p = float4(float(id) * scale, 0, 0, 1); return v; }
fragment float4 fs(V in [[stage_in]]) { return tint ? float4(1, 0, 0, 1) : float4(0, 1, 0, 1); }
