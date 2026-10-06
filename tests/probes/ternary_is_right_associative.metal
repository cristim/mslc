// EXPECT: valid
// DISASM-MATCH: = OpSelect %int %[0-9]+ %int_2 %int_3
//
// `a ? 1 : b ? 2 : 3` is `a ? 1 : (b ? 2 : 3)`.
kernel void ternary_is_right_associative(device int* out [[buffer(0)]], constant int* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    int a = in[0];
    int b = in[1];
    out[i] = a ? 1 : b ? 2 : 3;
}
