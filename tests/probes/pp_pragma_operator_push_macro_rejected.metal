// EXPECT: error #pragma push_macro is not supported
// _Pragma("push_macro(...)") changes which macros exist, exactly as the #pragma form does,
// so it is refused rather than ignored.
kernel void pp_pragma_operator_push_macro_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  _Pragma("push_macro(\"X\")")
  out[i] = i;
}
