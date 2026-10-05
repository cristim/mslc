// EXPECT: error a preprocessor directive inside the arguments of macro "ONE" is not supported
// Apple's compiler accepts a directive in the arguments, with a warning; mslc names it instead of guessing.
#define ONE(a) a
kernel void pp_macro_call_with_directive_inside_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = ONE(
#if 1
    1
#endif
); }
