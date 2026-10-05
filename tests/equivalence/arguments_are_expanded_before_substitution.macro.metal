#define BASE 4u
#define TWICE(x) ((x) + (x))
#define QUAD(x) TWICE(TWICE(x))
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = QUAD(BASE) + i; }
