#define SQ(x) ((x) * (x))
#define ADD(a, b) ((a) + (b))
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = ADD(SQ(i + 1u), 3u); }
