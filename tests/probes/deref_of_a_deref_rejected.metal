// EXPECT: error dereferencing a pointer to a pointer is not supported
//
kernel void deref_of_a_deref_rejected(device float* out [[buffer(0)]],
                                       device const float* in [[buffer(1)]])
{
    out[0] = **in;
}
