// EXPECT: error "k" in "k::x" is not a namespace the file declares
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = k::x; }
