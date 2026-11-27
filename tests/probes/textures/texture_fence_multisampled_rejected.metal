// EXPECT: error depth and multisampled textures have no fence
// Apple: no member named 'fence' on multisampled textures.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d_ms<float> a [[texture(0)]]) { a.fence(); }
