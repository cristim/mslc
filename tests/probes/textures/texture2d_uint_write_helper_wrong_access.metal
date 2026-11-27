// EXPECT: error which is not a texture2d<uint, access::write>
#include <metal_stdlib>
using namespace metal;
void store(texture2d<uint, access::write> dst) {}
kernel void hit(texture2d<float> src) { store(src); }
