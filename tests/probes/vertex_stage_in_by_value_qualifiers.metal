// EXPECT: valid
//
// Apple accepts the struct by value with or without const and thread.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[attribute(0)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_stage_in_by_value_qualifiers(const thread In in [[stage_in]]) { Out o; o.p = in.p; return o; }
