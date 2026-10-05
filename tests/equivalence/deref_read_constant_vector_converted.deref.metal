kernel void k(device float2* out [[buffer(0)]], constant uint2* size [[buffer(1)]])
{
    out[0] = float2(*size);
}
