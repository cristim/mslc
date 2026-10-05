kernel void k(device float* out [[buffer(0)]], device const float* in [[buffer(1)]])
{
    out[0] = 2.0 * *in - -*in * *in;
}
