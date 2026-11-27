// EXPECT: error is [[position]], which a vertex function
//
// Apple rejects a position field in a vertex stage_in struct.
#include <metal_stdlib>
using namespace metal;
struct In { float4 a [[position]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_position_field_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
