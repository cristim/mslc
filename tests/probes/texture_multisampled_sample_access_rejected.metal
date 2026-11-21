// EXPECT: error a multisampled texture is read-only
// Apple: static_assert failure for texture2d_ms with access::sample.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d_ms<float, access::sample> a [[texture(0)]]) {}
