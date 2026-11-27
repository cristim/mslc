// EXPECT: error with a three-component vector is lowered only from another three-component vector
// Metal stores a three-component vector as four, so float3 and uint4 may be the same size there; mslc
// takes three-component vectors only with each other.
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint4* out [[buffer(0)]], device const float3* in [[buffer(1)]]) { out[0] = as_type<uint4>(in[0]); }
