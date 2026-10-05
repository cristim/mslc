// EXPECT: error integer literal is too large to be represented in any integer type
// Past 2^128.
#if 0x100000000000000000000000000000000
#endif
kernel void pp_if_literal_too_large_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
