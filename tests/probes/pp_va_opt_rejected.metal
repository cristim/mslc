// EXPECT: error "__VA_OPT__" is a builtin of Apple's compiler that mslc does not provide
// Named, not silently treated as a name.
#define MAYBE(a, ...) a __VA_OPT__(,) __VA_ARGS__
#define USE MAYBE(1, 2)
kernel void pp_va_opt_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = USE; }
