// EXPECT: valid
// DISASM-MATCH: = OpISub %int %[0-9]+ %int_3.[ ]+OpStore %[0-9]+ %[0-9]+
//
// x -= 3 stores the result of the operation. A shift whose result is discarded
// validates and leaves x as it was, so the store has to follow the operation.
kernel void compound_subtract(device int *out [[buffer(0)]],
                              uint i [[thread_position_in_grid]])
{
    int x = out[i];
    x -= 3;
    out[i] = x;
}
