// EXPECT: error macro "ONE" requires 1 arguments, but 3 were given
// Extra arguments are not dropped.
#define ONE(a) a
kernel void pp_macro_too_many_arguments_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = ONE(1, 2, 3); }
