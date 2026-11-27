// EXPECT: valid
// DISASM: OpConstant %int 2525
// DISASM-NOT: OpConstant %int 2626
// Operators, precedence, hex, suffixes, shifts and the ternary in an #if.
kernel void pp_if_integer_arithmetic(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if (1 << 4) == 16 && 7 / 2 == 3 && -7 % 4 == -3 && 0xFFu == 255 && (1 ? 2 : 3) == 2 && 1 + 2 * 3 == 7 && (6 ^ 3) == 5 && ~0 == -1 && 010 == 8 && 0b11 == 3 && 5ul == 5 && (7 & 3) == 3 && (5 | 2) == 7 && 10 - 3 - 2 == 5 && 100 / 10 / 5 == 2 && !(2 < 1) && 2 <= 2 && 3 >= 3 && 3 > 2 && 1 != 2
    out[i] = 2525;
#else
    out[i] = 2626;
#endif
 }
