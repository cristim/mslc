// EXPECT: error __VA_ARGS__ can only appear in the expansion of a variadic macro
// __VA_ARGS__ needs a variadic macro.
#define BAD(a) __VA_ARGS__
kernel void pp_va_args_outside_a_variadic_macro_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
