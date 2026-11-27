// EXPECT: error is [[color(n)]], which only a fragment function reads

#include <metal_stdlib>
using namespace metal;

vertex float4 framebuffer_fetch_in_vertex_rejected(float4 c [[color(0)]]) { return c; }
