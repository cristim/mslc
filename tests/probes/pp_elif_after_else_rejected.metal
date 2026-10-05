// EXPECT: error #elif after #else
// #elif cannot follow #else.
#if 0
#else
#elif 1
#endif
kernel void pp_elif_after_else_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
