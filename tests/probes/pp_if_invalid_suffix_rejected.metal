// EXPECT: error invalid suffix "z" on integer constant
// 10z is not a number.
#if 10z
#endif
kernel void pp_if_invalid_suffix_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
