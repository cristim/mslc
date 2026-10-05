// EXPECT: valid
// DISASM: OpConstant %int 7070
// DISASM-NOT: OpConstant %int 7171
// Unsigned / and %, and unary plus, in an #if.
kernel void pp_if_unsigned_division_unary_plus(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if (-1 / 2u) > 0 && (10 % 3u) == 1 && (-7 % 3) == -1 && (-7 / 2) == -3 && +5 == 5 && -(+5) == -5
    out[i] = 7070;
#else
    out[i] = 7171;
#endif
 }
