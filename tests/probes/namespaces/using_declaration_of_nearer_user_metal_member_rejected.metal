// EXPECT: error no member named "clamp" in the namespace "N::metal"
// N::metal hides the library's metal, so "using metal::clamp;" needs a clamp in N::metal
// (Apple: no member named clamp in namespace N::metal).
#include <metal_stdlib>
namespace N { namespace metal { } using metal::clamp; }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = 1; }
