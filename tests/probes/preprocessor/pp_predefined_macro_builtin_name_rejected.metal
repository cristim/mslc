// EXPECT: error builtin
// DEFINE: __LINE__=3
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1; }
