// EXPECT: error is [[color(n)]], which Apple's compiler takes by value

#include <metal_stdlib>
using namespace metal;

fragment float4 framebuffer_fetch_pointer_rejected(thread float4& c [[color(0)]]) { return c; }
