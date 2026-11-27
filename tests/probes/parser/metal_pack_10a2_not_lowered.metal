// EXPECT: error unpack_unorm10a2_to_float
// The 10a2 and 565 forms (Metal 4) are not lowered yet.
#include <metal_stdlib>
using namespace metal;
kernel void k(device const uint* in [[buffer(0)]], device float4* out [[buffer(1)]]) { out[0] = unpack_unorm10a2_to_float(in[0]); }
