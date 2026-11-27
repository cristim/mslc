// EXPECT: valid
// DISASM-MATCH: = OpINotEqual %bool %[_0-9a-zA-Z]+ %int_0[^_0-9a-zA-Z]
// DISASM-MATCH: = OpLogicalNot %bool
//
// !n is !(n != 0).
kernel void not_of_int_is_compared_with_zero(device uint *out [[buffer(0)]], constant int *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (!v[i]) {
        out[i] = 1u;
    }
}
