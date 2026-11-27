// EXPECT: error cannot store through "in", which is in the constant address space or declared const
//
kernel void store_through_a_const_pointer_rejected(device const float* in [[buffer(0)]])
{
    in[0] = 1.0;
}
