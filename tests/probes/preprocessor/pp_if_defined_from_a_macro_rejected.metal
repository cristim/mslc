// EXPECT: error 'defined' produced by macro expansion is not supported
// Apple's compiler evaluates it with a warning; the behaviour is undefined in C, so mslc names it.
#define IS_SET defined(SET)
#if IS_SET
#endif
kernel void pp_if_defined_from_a_macro_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
