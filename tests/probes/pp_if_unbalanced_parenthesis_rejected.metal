// EXPECT: error missing ')' in the #if expression
// An open parenthesis.
#if (1
#endif
kernel void pp_if_unbalanced_parenthesis_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
