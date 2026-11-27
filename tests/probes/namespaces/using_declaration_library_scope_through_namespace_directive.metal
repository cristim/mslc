// EXPECT: valid
// N::precise::sqrt through a directive of metal in N.
#include <metal_stdlib>
namespace N { using namespace metal; } using N::precise::sqrt;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = 1; }
