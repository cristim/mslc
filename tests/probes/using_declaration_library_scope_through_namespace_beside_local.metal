// EXPECT: valid
// The same through a namespace that nominates metal.
#include <metal_stdlib>
using namespace metal;
namespace N { using namespace metal; } kernel void kern(device float* out [[buffer(0)]]) { float access = 1; using N::access::read; out[0] = access; }
