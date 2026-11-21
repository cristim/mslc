// EXPECT: error a multisampled texture is read-only
// Apple: no access::write for texture2d_ms.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d_ms<float, access::write> a [[texture(0)]]) {}
