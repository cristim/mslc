// EXPECT: valid
// DISASM: OpConstant %int 2222
// DISASM-NOT: OpConstant %int 1111
// DISASM-NOT: OpConstant %int 3333
// Replaces conditional_is_an_error: #if used to be rejected whole, and now picks one branch.
// The directives are inside the function body, which only a preprocessor can allow.
#define MODE 2
kernel void pp_conditional_selects_branch(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if MODE == 1
    out[i] = 1111;
#elif MODE == 2
    out[i] = 2222;
#else
    out[i] = 3333;
#endif
 }
