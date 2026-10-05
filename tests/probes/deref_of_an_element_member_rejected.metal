// EXPECT: error the operand of unary '*' has to be a pointer parameter
//
// The same rejection as deref_of_an_element_rejected, reached through a member
// access, which walks the chain by a different path than a plain read.
struct Pair {
    float4 a;
    float b;
};

kernel void deref_of_an_element_member_rejected(device float* out [[buffer(0)]],
                                                device const Pair* in [[buffer(1)]])
{
    out[0] = (*in[0]).b;
}
