// EXPECT: valid
// DISASM-MATCH: = OpIAdd %int %[0-9]+ %int_1
// DISASM-MATCH: = OpSLessThan %bool
//
// j++ as the increment of a for loop.
kernel void step_in_a_for_loop_increment(device int *out [[buffer(0)]],
                                         uint i [[thread_position_in_grid]])
{
    int s = 0;
    for (int j = 0; j < 10; j++) {
        s += j;
    }
    out[i] = s;
}
