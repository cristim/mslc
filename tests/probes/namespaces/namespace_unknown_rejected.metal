// EXPECT: error "Q" in "Q::f" is not a namespace the file declares
#include <metal_stdlib>
using namespace metal;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = Q::f(1.0); }
