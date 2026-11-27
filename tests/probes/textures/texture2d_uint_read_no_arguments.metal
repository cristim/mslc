// EXPECT: error read takes a coordinate
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::read> src) { src.read(); }
