// EXPECT: valid
// DISASM-ORDER: = OpPhi %bool %true
// DISASM-ORDER: = OpPhi %bool %false
//
// A right operand that branches itself ends in a different block than it began
// in, and the OpPhi of the outer && has to name the block it ends in. The
// inner || joins first, and the outer && joins on its result.
kernel void logical_and_with_nested_or_right_operand(device uint *out [[buffer(0)]], constant float *v [[buffer(1)]], constant int *n [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    int j = int(i);
    if (j < n[0] && (v[j] > 0.0 || v[j + 1] > 0.0)) {
        out[i] = 1u;
    }
}
