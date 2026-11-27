// EXPECT: valid
// DISASM: OpConstant %int 5252
// A macro with an empty body expands to nothing.
#define EMPTY
kernel void pp_empty_macro_vanishes(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = EMPTY 5252; }
