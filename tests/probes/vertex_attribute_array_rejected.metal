// EXPECT: error is a float4[2], which mslc does not pass between stages yet
//
// Apple: the attribute cannot be applied to an array type. mslc parses the array
// member and diagnoses it where the stage interface is built.
#include <metal_stdlib>
using namespace metal;
struct In { float4 a[2] [[attribute(0)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_array_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
