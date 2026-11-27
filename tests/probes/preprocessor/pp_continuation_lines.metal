// EXPECT: valid
// DISASM: OpConstant %int 4848
// DISASM: OpConstant %int 4949
// A backslash-newline joins lines in a directive and in the middle of a token.
#define SUM(a, b) \
    ((a) + \
     (b))
kernel void pp_continuation_lines(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = SUM(4848, 49\
49); }
