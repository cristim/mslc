// EXPECT: error taking an address with '&' is not supported
//
kernel void address_of_rejected(device float* out [[buffer(0)]],
                                device const float* in [[buffer(1)]])
{
    out[0] = *&in[0];
}
