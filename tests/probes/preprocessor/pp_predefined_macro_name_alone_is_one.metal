// EXPECT: valid
// DEFINE: ENABLED
#if ENABLED != 1
#error a macro given with no value is 1
#endif
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = ENABLED; }
