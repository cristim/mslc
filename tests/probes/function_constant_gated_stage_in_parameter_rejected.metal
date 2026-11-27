// EXPECT: error on a [[stage_in]] parameter is not lowered yet
// A gated stage input is not lowered yet.
#include <metal_stdlib>
using namespace metal;
constant bool g [[function_constant(0)]];
struct V { float4 p [[attribute(0)]]; };
vertex float4 v(V i [[stage_in, function_constant(g)]]) { return i.p; }
