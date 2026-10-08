// EXPECT: error overloading "store" is not lowered yet
#include <metal_stdlib>
using namespace metal;
void store(uint dst);
void store(texture2d<uint, access::write> dst) {}
kernel void hit() {}
