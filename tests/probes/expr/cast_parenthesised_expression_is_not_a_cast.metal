// EXPECT: valid
// DISASM: = OpIMul %int
//
// `(n) * 2` is a product with a parenthesised left operand.
kernel void cast_parenthesised_expression_is_not_a_cast(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    int n = int(i);
    out[i] = (n) * 2;
}
