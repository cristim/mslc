// EXPECT: error extra tokens at the end of #ifdef
// #ifdef A && B tests only A in C, and a mistaken && is easy to miss, so the extra tokens are refused (Apple warns).
#ifdef A && B
#endif
kernel void pp_ifdef_with_extra_tokens_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
