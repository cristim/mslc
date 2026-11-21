// RUN: vertex=vs fragment=fs
// DIFFCHECK-POSITION: pos
// Every fragment is discarded: neither backend writes a pixel, so the comparison matches
// vacuously. The zero-output rule must reject it.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; };
vertex float4 vs(VIn in [[stage_in]]) { return float4(in.pos, 0.5, 1.0); }
fragment float4 fs() { discard_fragment(); return float4(1.0); }
