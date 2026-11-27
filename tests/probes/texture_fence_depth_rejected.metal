// EXPECT: error depth and multisampled textures have no fence
// Apple: no member named 'fence' on depth textures.
#include <metal_stdlib>
using namespace metal;
kernel void hit(depth2d<float> a [[texture(0)]]) { a.fence(); }
