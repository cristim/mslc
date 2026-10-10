// EXPECT: error has to be a uint2 or a ushort2
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::read> src) { src.read(int2(0)); }
