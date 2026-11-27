// EXPECT: error metal::precise::dot
#include <metal_stdlib>
using namespace metal;
kernel void math_precise_non_member_rejected(device const float3* in [[buffer(0)]], device float* out [[buffer(1)]])
{
    out[0] = precise::dot(in[0], in[1]);
}
