// EXPECT: error has no [[attribute(n)]]
//
// A plain struct is not a vertex input.
#include <metal_stdlib>
using namespace metal;
struct In { float4 a; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_no_attribute_field_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
