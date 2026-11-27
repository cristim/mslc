// EXPECT: valid
// DISASM-ORDER: OpBranchConditional
// DISASM-ORDER: = OpLoad %float
// DISASM-ORDER: = OpLoad %float
// DISASM-ORDER: = OpPhi %bool %false
// DISASM-MATCH: = OpPhi %bool %false %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+
// DISASM-NO-MATCH: OpLogicalAnd
//
// The right operand of && is evaluated only when the left is true: the only
// float load, v[j], sits between the guard's branch and the OpPhi that joins
// the two paths.
kernel void logical_and_short_circuits(device uint *out [[buffer(0)]], constant float *v [[buffer(1)]], constant int *n [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    int j = int(i);
    if (j < n[0] && v[j] > 0.0) {
        out[i] = 1u;
    }
}
