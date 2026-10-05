// EXPECT: error #else after #else
// One #else per #if.
#if 0
#else
#else
#endif
kernel void pp_else_after_else_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
