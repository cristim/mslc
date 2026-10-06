// EXPECT: error expected a type, found [
//
// Apple: the attribute cannot be applied to an array type. mslc has no array
// field in a struct and diagnoses the bracket at the parse.
#include <metal_stdlib>
using namespace metal;
struct In { float4 a[2] [[attribute(0)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_array_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
