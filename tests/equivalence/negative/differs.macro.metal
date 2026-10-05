#define N 1u
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = N; }
