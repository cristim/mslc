// EXPECT: error cannot store through "x", which is in the constant address space or declared const
kernel void store_to_trailing_const_local_rejected(device float* o [[buffer(0)]])
{
    float const x = 1.0;
    x = 2.0;
    o[0] = x;
}
