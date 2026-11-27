// EXPECT: error division by zero in the #if expression
// An evaluated division by zero is an error.
#if 1 / 0
#endif
kernel void pp_if_division_by_zero_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
