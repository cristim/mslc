// EXPECT: error is not lowered yet
#include <metal_stdlib>
using namespace metal;
kernel void hit(texturecube<uint, access::write> dst) {}
