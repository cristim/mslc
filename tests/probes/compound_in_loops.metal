// EXPECT: valid
// DISASM-MATCH: = OpIAdd %int %[0-9]+ %int_3
// DISASM-MATCH: = OpShiftLeftLogical %int %[0-9]+ %int_1
// DISASM-MATCH: = OpShiftRightArithmetic %int %[0-9]+ %int_1
//
// A compound assignment as a for-loop increment and in the bodies of a for and a while loop.
kernel void compound_in_loops(device int *out [[buffer(0)]],
                              uint i [[thread_position_in_grid]])
{
    int s = 0;
    for (int j = 0; j < 10; j += 3) {
        s <<= 1;
    }
    int w = out[i];
    while (w > 0) {
        w >>= 1;
        s -= 1;
    }
    out[i] = s;
}
