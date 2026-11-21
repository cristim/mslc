// RUN: vertex=vs fragment=fs
// A stage_in attribute with no DIFFCHECK-POSITION: the harness cannot know where the geometry goes.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; };
vertex float4 vs(VIn in [[stage_in]]) { return float4(in.pos, 0.5, 1.0); }
fragment float4 fs() { return float4(1.0); }
