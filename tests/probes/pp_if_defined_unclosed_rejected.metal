// EXPECT: error missing ')' after 'defined'
// defined( needs its parenthesis.
#if defined(A
#endif
kernel void pp_if_defined_unclosed_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
