// EXPECT: error pointer arithmetic such as "*(p + i)" is not supported
//
kernel void deref_of_pointer_arithmetic_rejected(device float* out [[buffer(0)]],
                                                  device const float* in [[buffer(1)]])
{
    out[0] = *(in + 1);
}
