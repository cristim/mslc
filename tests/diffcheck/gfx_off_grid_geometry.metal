// RUN: vertex=vs fragment=fs
// DIFFCHECK-POSITION: pos
// DIFFCHECK-GEOMETRY: 2.3,2 20,2 2,20
// A vertex off the 1/256 pixel grid: refused, the backends snap it differently.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; };
vertex float4 vs(VIn in [[stage_in]]) { return float4(in.pos, 0.5, 1.0); }
fragment float4 fs() { return float4(1.0); }
