// EXPECT: error as_type<float2x2> is not lowered: mslc reinterprets as a scalar or a vector
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], device const float4* in [[buffer(1)]]) { out[0] = as_type<float2x2>(in[0])[0][0]; }
