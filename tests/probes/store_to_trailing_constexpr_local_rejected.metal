// EXPECT: error cannot store through "c", which is in the constant address space or declared const
kernel void store_to_trailing_constexpr_local_rejected(device float* o [[buffer(0)]])
{
    float constexpr c = 3.0;
    c = 2.0;
    o[0] = c;
}
