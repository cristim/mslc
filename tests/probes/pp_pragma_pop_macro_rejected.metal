// EXPECT: error #pragma pop_macro is not supported
// pop_macro changes which macros exist, so it is refused rather than dropped.
#pragma pop_macro("X")
kernel void pp_pragma_pop_macro_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
