// EXPECT: error has no [[attribute(n)]]
//
// Apple rejects a stage_in struct that mixes attribute and plain fields.
#include <metal_stdlib>
using namespace metal;
struct In { float4 a [[attribute(0)]]; float4 b; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_field_without_attribute_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
