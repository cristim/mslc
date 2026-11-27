// EXPECT: valid
// DISASM-ORDER: OpLoopMerge
// DISASM-ORDER: = OpPhi %bool %false
//
// A condition that short-circuits spans several blocks, and OpLoopMerge has
// to stay in the loop header, so the condition follows it and the loop's own
// OpBranchConditional follows the OpPhi.
kernel void logical_and_in_while_condition(device uint *out [[buffer(0)]], constant float *v [[buffer(1)]], constant int *n [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    int j = 0;
    while (j < n[0] && v[j] > 0.0) {
        j = j + 1;
    }
    out[i] = uint(j);
}
