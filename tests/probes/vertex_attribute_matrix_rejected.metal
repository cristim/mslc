// EXPECT: error which Apple's compiler does not allow as a vertex attribute
//
// Apple: a matrix is not valid for the attribute.
#include <metal_stdlib>
using namespace metal;
struct In { float4x4 a [[attribute(0)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_matrix_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
