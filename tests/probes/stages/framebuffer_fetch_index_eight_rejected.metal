// EXPECT: error [[color(8)]], and Apple's compiler allows indices 0 to 7

#include <metal_stdlib>
using namespace metal;

fragment float4 framebuffer_fetch_index_eight_rejected(float4 c [[color(8)]]) { return c; }
