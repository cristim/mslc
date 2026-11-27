// EXPECT: error cannot store through "c", which is in the constant address space or declared const
struct S { float a; };
kernel void store_through_trailing_const_reference_member_rejected(device S const &c [[buffer(0)]])
{
    c.a = 1.0;
}
