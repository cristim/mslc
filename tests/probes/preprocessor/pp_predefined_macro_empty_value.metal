// EXPECT: valid
// DEFINE: EMPTY=
#ifndef EMPTY
#error a macro given an empty value is still defined
#endif
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = EMPTY 5; }
