// EXPECT: error taking an address with '&' is not supported
//
// Only an atomic_*_explicit function takes the address of an element.
kernel void address_of_rejected(device float* out [[buffer(0)]],
                                device const float* in [[buffer(1)]])
{
    (void)&in[0];
    out[0] = in[0];
}
