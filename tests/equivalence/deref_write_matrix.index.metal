kernel void k(device float4x4* out [[buffer(0)]], device const float4x4* in [[buffer(1)]])
{
    out[0] = in[0];
}
