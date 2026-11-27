// EXPECT: valid
// DISASM-ORDER: OpLoopMerge
// DISASM-ORDER: = OpPhi %bool %false
//
// The same loop shape for a for statement: the condition's blocks come after
// OpLoopMerge, and the continue block still holds the increment.
kernel void logical_and_in_for_condition(device uint *out [[buffer(0)]], constant float *v [[buffer(1)]], constant int *n [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    uint count = 0u;
    for (int j = 0; j < n[0] && v[j] > 0.0; j = j + 1) {
        count = count + 1u;
    }
    out[i] = count;
}
