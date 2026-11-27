// EXPECT: error extra tokens at the end of the #include
// Apple warns; mslc does not drop tokens.
#include "include/pp_defs.h" junk
kernel void pp_include_with_extra_tokens_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
