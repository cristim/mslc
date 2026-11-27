// EXPECT: valid
// DISASM-MATCH: = OpBitcast %uint %int_1
// DISASM-MATCH: = OpISub %uint %[0-9]+ %[0-9]+
//
// uint u; u -= 1 subtracts as uint: the int literal is converted, not the other way round.
kernel void compound_unsigned_literal_rhs(device uint *out [[buffer(0)]],
                                          uint i [[thread_position_in_grid]])
{
    uint x = out[i];
    x -= 1;
    out[i] = x;
}
