// EXPECT: valid
// DISASM: OpConstant %int 3131
// DISASM-NOT: OpConstant %int 3232
// As in C, a name that is not a macro is 0 in an #if. true and false are keywords.
kernel void pp_if_undefined_name_is_zero(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if UNDEFINED_NAME == 0 && !UNDEFINED_NAME && true && !false
    out[i] = 3131;
#else
    out[i] = 3232;
#endif
 }
