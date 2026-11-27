// EXPECT: valid
// DISASM-ORDER: OpLoopMerge
// DISASM-ORDER: = OpPhi %bool %false
//
// The && is the operand of a ! in the loop condition, and a ! holds its operand
// as its left child.
kernel void logical_and_in_while_condition_not_operand(device uint *out [[buffer(0)]], constant int *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool a = v[0] > 0;
    bool b = v[1] > 0;
    uint count = 0u;
    while (!(a && b)) {
        count = count + 1u;
        a = b;
    }
    out[i] = count;
}
