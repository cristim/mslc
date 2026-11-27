// EXPECT: valid
// A scalar parameter and a texture parameter of one name are two functions (Apple
// accepts it, nothing calls either).
#include <metal_stdlib>
using namespace metal;
void store(uint dst);
void store(texture2d<uint, access::write> dst) {}
kernel void hit() {}
