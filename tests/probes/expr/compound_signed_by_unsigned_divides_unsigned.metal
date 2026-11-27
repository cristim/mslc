// EXPECT: valid
// DISASM-MATCH: = OpUDiv %uint %[0-9]+ %uint_2
// DISASM-NO-MATCH: OpSDiv
//
// int x; x /= 2u divides as unsigned, because an int beside a uint makes both uint. -1 / 2u is
// 2147483647, where a signed division would give 0.
kernel void compound_signed_by_unsigned_divides_unsigned(device int *out [[buffer(0)]],
                                                         uint i [[thread_position_in_grid]])
{
    int x = out[i];
    x /= 2u;
    out[i] = x;
}
