// EXPECT: error has [[stencil]], which is only valid on a fragment output
#include <metal_stdlib>
using namespace metal;
struct V { float4 pos [[position]]; uint s [[stencil]]; };
vertex V v(uint id [[vertex_id]]) { V o; o.pos = float4(0.0f); o.s = id; return o; }
