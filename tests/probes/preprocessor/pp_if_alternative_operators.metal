// EXPECT: valid
// DISASM: OpConstant %int 6868
// DISASM-NOT: OpConstant %int 6969
// and, or, not, bitand, bitor, xor, compl and not_eq are the operators in an #if.
kernel void pp_if_alternative_operators(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if (1 and 1) && (0 or 1) && (not 0) && ((6 bitand 3) == 2) && ((4 bitor 1) == 5) && ((6 xor 3) == 5) && (compl 0 == -1) && (1 not_eq 2)
    out[i] = 6868;
#else
    out[i] = 6969;
#endif
 }
