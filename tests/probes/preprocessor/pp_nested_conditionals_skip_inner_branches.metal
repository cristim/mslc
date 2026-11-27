// EXPECT: valid
// DISASM: OpConstant %int 2424
// DISASM-NOT: OpConstant %int 2121
// DISASM-NOT: OpConstant %int 2323
// The inner #else of a skipped group must not be taken.
kernel void pp_nested_conditionals_skip_inner_branches(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if 0
#if 1
    out[i] = 2121;
#else
    out[i] = 2323;
#endif
#else
    out[i] = 2424;
#endif
 }
