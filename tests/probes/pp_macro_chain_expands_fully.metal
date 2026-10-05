// EXPECT: valid
// DISASM: OpConstant %int 4343
// A macro whose body names another macro is expanded again.
#define OUTER MIDDLE
#define MIDDLE INNER
#define INNER 4343
kernel void pp_macro_chain_expands_fully(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = OUTER; }
