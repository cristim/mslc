// EXPECT: error which mslc does not pass between stages yet
//
// Apple accepts a short attribute; mslc does not widen narrow integers across the stage interface.
#include <metal_stdlib>
using namespace metal;
struct In { short2 a [[attribute(0)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_narrow_integer_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
