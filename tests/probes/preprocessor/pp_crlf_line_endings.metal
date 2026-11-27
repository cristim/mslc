// EXPECT: valid
// DISASM: OpConstant %int 5555
// CRLF endings, with a continuation, a conditional and a comment.
#define SUM \
    5555
#if SUM == 5555 // trailing comment
kernel void pp_crlf_line_endings(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = SUM; }
#endif
