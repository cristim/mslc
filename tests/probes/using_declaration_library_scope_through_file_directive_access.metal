// EXPECT: valid
// access::read through "using namespace metal;".
#include <metal_stdlib>
using namespace metal; using access::read;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = 1; }
