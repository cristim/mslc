// EXPECT: error have to be the vertex function's first ones, in order
//
// Reading attributes does not exempt the vertex outputs from the pairing check:
// the fragment lists the two colours the other way round from the vertex
// function's returned struct.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[attribute(0)]]; };
struct Out { float4 p [[position]]; float4 a; float4 b; };
struct FragIn { float4 p [[position]]; float4 b; float4 a; };
vertex Out vs(In in [[stage_in]]) { Out o; o.p = in.p; o.a = in.p; o.b = in.p; return o; }
fragment float4 fs(FragIn in [[stage_in]]) { return in.a; }
