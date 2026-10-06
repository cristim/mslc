// EXPECT: valid
// DISASM-ORDER: OpLoopMerge
// DISASM-ORDER: = OpPhi %bool %false
//
// The && is the right operand of an addition in a for condition.
kernel void logical_and_in_for_condition_right_operand(device uint *out [[buffer(0)]], constant int *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool a = v[0] > 0;
    bool b = v[1] > 0;
    uint count = 0u;
    for (int j = 0; j < 3 + int(a && b); j = j + 1) {
        count = count + 1u;
    }
    out[i] = count;
}
