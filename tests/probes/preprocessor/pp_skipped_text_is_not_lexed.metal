// EXPECT: valid
// DISASM: OpConstant %int 3636
// Text in a skipped group needs no valid tokens: an apostrophe, a lone quote, an unknown directive.
kernel void pp_skipped_text_is_not_lexed(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if 0
    it's a "quote and an @ sign
#bogus_directive
#error not reached
#else
    out[i] = 3636;
#endif
 }
