// EXPECT: error which mslc does not lower as a vertex attribute
//
// Apple accepts a bool attribute; mslc has no vertex format to give it and reports it.
#include <metal_stdlib>
using namespace metal;
struct In { bool a [[attribute(0)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_bool_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
