// EXPECT: error a multisampled texture is read-only
// Apple refuses access::write on depth2d_ms.
#include <metal_stdlib>
using namespace metal;
kernel void hit(depth2d_ms<float, access::write> a [[texture(0)]]) {}
