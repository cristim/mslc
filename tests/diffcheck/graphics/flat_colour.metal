// RUN: vertex=vs fragment=fs cases=2
// DIFFCHECK-POSITION: pos
// A constant colour: the whole result is exact, so a wrong output location, component order
// or constant changes it.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; };
vertex float4 vs(VIn in [[stage_in]]) { return float4(in.pos, 0.5, 1.0); }
fragment float4 fs() { return float4(0.25, 0.5, 0.75, 0.125); }
