// EXPECT: error macro "PAIR" requires 2 arguments, but 1 was given
// Argument count is checked.
#define PAIR(a, b) a b
kernel void pp_macro_too_few_arguments_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = PAIR(1); }
