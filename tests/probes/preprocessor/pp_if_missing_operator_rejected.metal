// EXPECT: error missing a binary operator before "2"
// Two values with nothing between.
#if 1 2
#endif
kernel void pp_if_missing_operator_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
