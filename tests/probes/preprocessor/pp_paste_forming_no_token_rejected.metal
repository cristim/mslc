// EXPECT: error does not give a valid preprocessing token
// + ## 1 is not a token.
#define JOIN(a, b) a##b
kernel void pp_paste_forming_no_token_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = JOIN(+, 1); }
