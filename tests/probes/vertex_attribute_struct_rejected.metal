// EXPECT: error which Apple's compiler does not allow as a vertex attribute
//
// Apple: a struct is not valid for the attribute.
#include <metal_stdlib>
using namespace metal;
struct Inner { float4 a; };
struct In { Inner a [[attribute(0)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_struct_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
