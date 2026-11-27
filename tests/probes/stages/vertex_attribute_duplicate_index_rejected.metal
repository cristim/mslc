// EXPECT: error reuses [[attribute(1)]]
//
// Apple: attribute index 1 is used more than once.
#include <metal_stdlib>
using namespace metal;
struct In { float4 a [[attribute(1)]]; float4 b [[attribute(1)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_duplicate_index_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
