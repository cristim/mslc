// EXPECT: error cannot store through "p", which is in the constant address space or declared const
//
// "device float const *p" is "const device float *p": the pointee is const.
kernel void store_through_trailing_const_pointer_rejected(device float const *p [[buffer(0)]])
{
    p[0] = 1.0;
}
