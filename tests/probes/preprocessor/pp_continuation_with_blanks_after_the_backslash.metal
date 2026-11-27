// EXPECT: valid
// DISASM: OpConstant %int 5454
// Apple's compiler splices a line whose backslash is followed by blanks, so mslc does too.
#define SPLIT 5454 + \ 	 
    0
kernel void pp_continuation_with_blanks_after_the_backslash(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = SPLIT; }
