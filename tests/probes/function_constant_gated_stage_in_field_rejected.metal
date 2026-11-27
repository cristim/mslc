// EXPECT: error is lowered on a [[color(n)]] field only
// A gated vertex input would change the vertex layout of a pipeline, which is not lowered yet.
#include <metal_stdlib>
using namespace metal;
constant bool g [[function_constant(0)]];
struct V { float4 p [[attribute(0)]]; float4 q [[attribute(1), function_constant(g)]]; };
vertex float4 v(V i [[stage_in]]) { return i.p; }
