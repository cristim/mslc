// EXPECT: valid
// DISASM: OpConstant %int 2929
// DISASM-NOT: OpConstant %int 3030
// -1 against an unsigned operand is the largest value, as in C.
kernel void pp_if_mixed_signedness_compares_as_unsigned(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if -1 > 0u && -1 < 0
    out[i] = 2929;
#else
    out[i] = 3030;
#endif
 }
