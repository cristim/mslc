// EXPECT: valid
// A block-scope "using namespace metal;" makes precise:: reachable for a using declaration.
#include <metal_stdlib>
using namespace metal;
kernel void kern(device float* out [[buffer(0)]]) { { using namespace metal; using precise::sqrt; } out[0] = 1; }
