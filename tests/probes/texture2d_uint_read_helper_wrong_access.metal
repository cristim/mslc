// EXPECT: error which is not a texture2d<uint, access::read>
#include <metal_stdlib>
using namespace metal;
void load(texture2d<uint, access::read> src) {}
kernel void hit(texture2d<uint, access::write> dst) { load(dst); }
