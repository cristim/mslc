// EXPECT: valid
// DISASM: OpConstant %int 5353
// A '#' that follows a comment on its line starts a directive.
/* a comment that
   spans lines */ #define AFTER_COMMENT 5353
kernel void pp_hash_after_a_multiline_comment_is_a_directive(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = AFTER_COMMENT; }
