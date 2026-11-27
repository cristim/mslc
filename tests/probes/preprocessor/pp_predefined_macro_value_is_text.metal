// EXPECT: valid
// DEFINE: COUNT=2+3
// The value is text that expands in place, like any macro, not a number
#if COUNT * 2 != 8
#error 2+3 times 2 is 2+3*2
#endif
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = COUNT; }
