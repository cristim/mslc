// EXPECT: error a reference
#include <metal_stdlib>
using namespace metal;
void store(texture2d<uint, access::write>& dst) {}
kernel void hit() {}
