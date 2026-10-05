// EXPECT: valid
// DISASM: OpConstant %int 2727
// DISASM-NOT: OpConstant %int 2828
// Apple's front end reports __INTMAX_WIDTH__ as 128, so #if arithmetic does not wrap at 2^64.
kernel void pp_if_integers_are_wider_than_64_bits(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if (1 << 64) != 0 && 0xFFFFFFFFFFFFFFFF != -1 && 18446744073709551616 > 18446744073709551615 && (1 << 127) < 0 && (1 << 128) == 0 && (-1 >> 200) == -1 && (-1u >> 128) == 1
    out[i] = 2727;
#else
    out[i] = 2828;
#endif
 }
