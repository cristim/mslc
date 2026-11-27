// EXPECT: valid
// DISASM-MATCH: = OpUDiv %uint %[0-9]+ %uint_3.[ ]+OpStore %[0-9]+ %[0-9]+
//
// x /= 3u stores the result of the operation. A shift whose result is discarded
// validates and leaves x as it was, so the store has to follow the operation.
kernel void compound_divide_unsigned(device uint *out [[buffer(0)]],
                                     uint i [[thread_position_in_grid]])
{
    uint x = out[i];
    x /= 3u;
    out[i] = x;
}
