// EXPECT: error #pragma push_macro is not supported
// The pragma is kept through argument pre-expansion and acted on at the rescan.
#define ID(x) x
kernel void pp_pragma_operator_push_macro_in_an_argument_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  ID(_Pragma("push_macro(\"X\")"))
  out[i] = i;
}
