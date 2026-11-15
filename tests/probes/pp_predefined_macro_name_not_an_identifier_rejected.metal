// EXPECT: error is not an identifier
// DEFINE: 1BAD=1
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1; }
