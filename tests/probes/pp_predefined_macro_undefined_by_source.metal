// EXPECT: valid
// DEFINE: GONE=1
#undef GONE
#ifdef GONE
#error a predefined macro can be #undef'd
#endif
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1; }
