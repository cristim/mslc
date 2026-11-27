// EXPECT: valid
// "using namespace metal;" makes precise:: and access:: reachable unqualified.
#include <metal_stdlib>
using namespace metal; using precise::sqrt;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = 1; }
