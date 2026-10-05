// EXPECT: valid
// DISASM: OpConstant %int 5050
// The arguments of a macro call may span lines.
#define SECOND(a, b) b
kernel void pp_macro_call_across_lines(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = SECOND(1,
    5050
); }
