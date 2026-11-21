// EXPECT: error is in the device address space
#include <metal_stdlib>
using namespace metal;
float read(device float &c) { return c; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = read(o[1]); }
