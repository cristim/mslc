// EXPECT: error overloading "store" is not lowered yet
#include <metal_stdlib>
using namespace metal;
void store(texture2d<float> dst);
void store(texturecube<float> dst) {}
kernel void hit() {}
