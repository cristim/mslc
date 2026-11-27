// EXPECT: error as_type<uint4> of a three-component vector is lowered only to another three-component vector
// Metal stores a float3 as four floats, so float3 and uint4 are the same size, but the fourth
// component a float3 is stored with has no value to reinterpret.
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint4* out [[buffer(0)]], device const float3* in [[buffer(1)]]) { out[0] = as_type<uint4>(in[0]); }
