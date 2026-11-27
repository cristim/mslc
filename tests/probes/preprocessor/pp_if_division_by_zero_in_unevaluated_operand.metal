// EXPECT: valid
// DISASM: OpConstant %int 3331
// DISASM: OpConstant %int 3332
// DISASM: OpConstant %int 3333
// DISASM: OpConstant %int 3334
// The right side of && and || and the unchosen side of ?: are parsed, not evaluated. One #if each, so no result hides another.
kernel void pp_if_division_by_zero_in_unevaluated_operand(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if 0 && (1 / 0)
#else
    out[i] = 3331;
#endif
#if 1 || (1 % 0)
    out[i] = 3332;
#endif
#if (1 ? 1 : 1 / 0)
    out[i] = 3333;
#endif
#if (0 ? 1 / 0 : 1)
    out[i] = 3334;
#endif
 }
