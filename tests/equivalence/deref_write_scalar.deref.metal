kernel void k(device float* out [[buffer(0)]], device const float* in [[buffer(1)]])
{
    *out = *in + 1.0;
}
