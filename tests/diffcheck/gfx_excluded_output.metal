// RUN: vertex=vs fragment=fs
// DIFFCHECK-POSITION: pos
// DIFFCHECK-EXCLUDE: color1 the only non-zero target, excluded to show it is not counted
// color0 is written with zeros, color1 with real values.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; };
vertex float4 vs(VIn in [[stage_in]]) { return float4(in.pos, 0.5, 1.0); }
struct FOut { float4 a [[color(0)]]; float4 b [[color(1)]]; };
fragment FOut fs() { FOut o; o.a = float4(0.0); o.b = float4(0.5, 1.5, 2.5, 3.5); return o; }
