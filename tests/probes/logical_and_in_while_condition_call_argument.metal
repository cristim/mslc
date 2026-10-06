// EXPECT: valid
// DISASM-ORDER: OpLoopMerge
// DISASM-ORDER: = OpPhi %bool %false
//
// The && is an argument of a call inside the loop condition, so the condition
// still has to follow OpLoopMerge in its own block.
kernel void logical_and_in_while_condition_call_argument(device uint *out [[buffer(0)]], constant int *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool a = v[0] > 0;
    bool b = v[1] > 0;
    uint count = 0u;
    while (min(int(a && b), 1) > int(count)) {
        count = count + 1u;
    }
    out[i] = count;
}
