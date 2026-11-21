// EXPECT: error #pragma pop_macro is not supported
// As for push_macro, through a macro argument.
#define ID(x) x
kernel void pp_pragma_operator_pop_macro_in_an_argument_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  ID(_Pragma("pop_macro(\"X\")"))
  out[i] = i;
}
