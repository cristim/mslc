// EXPECT: valid
// N brings metal in, so N::access::read is the library's access::read.
#include <metal_stdlib>
namespace N { using namespace metal; } using N::access::read;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = 1; }
