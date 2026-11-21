// EXPECT: valid
// A prototype and a definition of one name with different texture parameters are two
// functions (Apple accepts it, nothing calls either).
#include <metal_stdlib>
using namespace metal;
void store(texture2d<float> dst);
void store(texturecube<float> dst) {}
kernel void hit() {}
