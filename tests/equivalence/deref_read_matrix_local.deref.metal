kernel void k(device float4* out [[buffer(0)]], device const float4x4* in [[buffer(1)]])
{
    float4x4 m = *in;
    out[0] = m[2];
}
