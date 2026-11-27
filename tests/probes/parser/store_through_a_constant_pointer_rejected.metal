// EXPECT: error cannot store through "size", which is in the constant address space or declared const
//
kernel void store_through_a_constant_pointer_rejected(constant uint2* size [[buffer(0)]])
{
    *size = uint2(1);
}
