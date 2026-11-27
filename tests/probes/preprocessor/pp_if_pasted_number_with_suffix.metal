// EXPECT: valid
// DISASM: OpConstant %int 5757
// 5 ## L is the single token 5L, which an #if reads as a long 5.
#define LONG(a) a ## L
kernel void pp_if_pasted_number_with_suffix(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if LONG(5) == 5 && LONG(7) > 6
    out[i] = 5757;
#endif
 }
