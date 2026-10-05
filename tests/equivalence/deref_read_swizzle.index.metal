kernel void k(device float4* out [[buffer(0)]], device const float4* in [[buffer(1)]])
{
    out[0] = float4(in[0].zyx, -in[0].w);
}
