// EXPECT: valid
// DISASM: = OpConstant %long 6000000000
//
// 3000000000 is a long, so the product is not reduced to 32 bits, and the
// constant has the two words a 64-bit literal takes.
constant long kValue = 3000000000 * 2;

kernel void file_scope_constant_long_is_two_words(device long* out [[buffer(0)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
