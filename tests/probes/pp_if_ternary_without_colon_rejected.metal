// EXPECT: error expected ':' in the #if conditional expression
// ? needs :.
#if 1 ? 2
#endif
kernel void pp_if_ternary_without_colon_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
