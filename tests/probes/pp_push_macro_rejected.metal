// EXPECT: error #pragma push_macro is not supported
// Ignoring it would change which macros exist.
#pragma push_macro("A")
kernel void pp_push_macro_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
