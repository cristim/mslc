// EXPECT: valid
// DISASM-MATCH: OpEntryPoint Vertex %[0-9]+ "vs" %[0-9]+ %[0-9]+ %gl_Position %[0-9]+
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "fs" %gl_FragCoord %[0-9]+
//
// The vertex function reads attributes and returns a struct a fragment function
// takes as [[stage_in]]; the pairing check still sees the vertex output.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[attribute(0)]]; float4 c [[attribute(1)]]; };
struct Out { float4 p [[position]]; float4 c; };
vertex Out vs(In in [[stage_in]]) { Out o; o.p = in.p; o.c = in.c; return o; }
fragment float4 fs(Out in [[stage_in]]) { return in.c; }
