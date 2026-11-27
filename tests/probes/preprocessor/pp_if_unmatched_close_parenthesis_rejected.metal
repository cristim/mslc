// EXPECT: error unmatched ')' in the #if expression
// A close parenthesis with no open one.
#if 1)
#endif
kernel void pp_if_unmatched_close_parenthesis_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
