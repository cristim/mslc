// EXPECT: valid
// DISASM: OpConstant %int 5151
// A block comment spanning lines does not end the directive it is in.
#define SPANNED 5151 /* the comment
 continues here */ + 1
kernel void pp_comment_ends_not_the_directive_line(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = SPANNED; }
