// EXPECT: error __VA_ARGS__ can only appear in the expansion of a variadic macro
// With a named variable argument, __VA_ARGS__ is not defined.
#define BAD(args...) __VA_ARGS__
kernel void pp_va_args_in_a_named_variadic_macro_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
