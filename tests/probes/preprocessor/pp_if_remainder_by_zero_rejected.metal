// EXPECT: error remainder by zero in the #if expression
// Likewise for %.
#if 1 % 0
#endif
kernel void pp_if_remainder_by_zero_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
