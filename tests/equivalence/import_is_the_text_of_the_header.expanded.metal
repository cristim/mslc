#define EQUIV_VALUE 4141
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 4141; }
