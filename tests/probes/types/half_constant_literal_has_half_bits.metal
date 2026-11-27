// EXPECT: valid
// DISASM-MATCH: = OpConstant %half 0x1\.4p\+1
//
// A file-scope half constant is the literal's own 16 bits. It was emitted with
// the low half of the float's bits, which is a different, wrong number.
constant half k = 2.5h;

kernel void half_constant_literal_has_half_bits(device half *out [[buffer(0)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = k;
}
