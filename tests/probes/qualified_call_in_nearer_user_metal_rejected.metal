// EXPECT: error no member named "sqrt" in the namespace "N::metal"
// The same for a qualified call: metal::sqrt in N finds N::metal only.
#include <metal_stdlib>
namespace N { namespace metal { } float f() { return metal::sqrt(4.0f); } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = 1; }
