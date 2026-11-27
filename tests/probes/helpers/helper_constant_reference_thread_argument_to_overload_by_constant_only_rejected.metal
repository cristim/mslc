// EXPECT: error not in the constant address space
#include <metal_stdlib>
using namespace metal;
float pick(const constant float &c) { return 222.0f; }
float pick(float a, float b) { return 1.0f; }
kernel void k(device float *o [[buffer(0)]]) { float t = 1.0f; o[0] = pick(t); }
