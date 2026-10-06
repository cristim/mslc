// EXPECT: valid
// DISASM-ORDER: OpBranchConditional
// DISASM-ORDER: = OpLoad %float
// DISASM-ORDER: = OpLoad %float
// DISASM-ORDER: = OpPhi %bool %true
// DISASM-MATCH: = OpPhi %bool %true %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+
// DISASM-NO-MATCH: OpLogicalOr
//
// The right operand of || is evaluated only when the left is false: the only
// float load, v[j], sits between the guard's branch and the OpPhi that joins
// the two paths, and the result when the left decides is true.
kernel void logical_or_short_circuits(device uint *out [[buffer(0)]], constant float *v [[buffer(1)]], constant int *n [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    int j = int(i);
    if (j >= n[0] || v[j] > 0.0) {
        out[i] = 1u;
    }
}
