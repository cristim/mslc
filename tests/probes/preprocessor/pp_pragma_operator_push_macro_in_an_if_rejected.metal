// EXPECT: error #pragma push_macro is not supported
// With no file to act on, push_macro is still refused rather than dropped.
#if _Pragma("push_macro(\"X\")") 1
#endif
kernel void pp_pragma_operator_push_macro_in_an_if_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
