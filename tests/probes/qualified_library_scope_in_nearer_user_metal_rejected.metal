// EXPECT: error in the namespace "N::metal"
// N::metal has no precise, and the library's does not show through it.
#include <metal_stdlib>
namespace N { namespace metal { } float f() { return metal::precise::sqrt(4.0f); } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = 1; }
