// EXPECT: error mslc lowers depth textures of float with access::sample or access::read
// Apple refuses depth2d with access::write.
#include <metal_stdlib>
using namespace metal;
kernel void hit(depth2d<float, access::write> a [[texture(0)]]) {}
