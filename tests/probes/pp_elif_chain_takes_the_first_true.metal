// EXPECT: valid
// DISASM: OpConstant %int 1818
// DISASM-NOT: OpConstant %int 1717
// DISASM-NOT: OpConstant %int 1919
// DISASM-NOT: OpConstant %int 2020
// Only the first true branch is taken, though later ones are also true.
#define V 3
kernel void pp_elif_chain_takes_the_first_true(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if V == 1
    out[i] = 1717;
#elif V == 3
    out[i] = 1818;
#elif V == 3
    out[i] = 1919;
#else
    out[i] = 2020;
#endif
 }
