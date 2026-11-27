// EXPECT: error no matching function for call to "total"
// vec<T, 4> deduces T from a vector of four components only: a float3 argument makes the template no candidate.
#include <metal_stdlib>
using namespace metal;
template <typename T> static T total(vec<T, 4> v) { return v.x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = total(float3(1)); }
