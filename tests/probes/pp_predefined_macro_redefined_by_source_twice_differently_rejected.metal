// EXPECT: error redefined differently
// DEFINE: VALUE=91
// The first #define replaces the predefined macro; defining it again differently is the usual error
#define VALUE 17
#define VALUE 18
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = VALUE; }
