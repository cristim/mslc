// EXPECT: error invalid digit '8' in octal constant
// 08 is not a number.
#if 08
#endif
kernel void pp_if_bad_octal_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
