// EXPECT: valid
// DISASM: OpConstant %int 3434
// DISASM-NOT: OpConstant %int 3535
// The expression of a later #elif is not evaluated once a branch was taken.
kernel void pp_elif_after_a_taken_branch_is_not_evaluated(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if 1
    out[i] = 3434;
#elif 1 / 0
    out[i] = 3535;
#elif not an expression at all
#endif
 }
