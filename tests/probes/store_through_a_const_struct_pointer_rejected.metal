// EXPECT: error cannot store through "in", which is in the constant address space or declared const
//
struct Pair {
    float4 a;
    float b;
};

kernel void store_through_a_const_struct_pointer_rejected(constant Pair* in [[buffer(0)]])
{
    (*in).b = 1.0;
}
