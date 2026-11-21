// EXPECT: valid
// The spelled metal::access::read stays valid beside a directive for metal.
#include <metal_stdlib>
using namespace metal; using metal::access::read;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = 1; }
