// EXPECT: error depth2d<half> is not lowered yet
// Apple refuses depth2d<half> too (invalid channel type).
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(depth2d<half> a [[texture(0)]]) { return float4(0); }
