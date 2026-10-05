// EXPECT: error #include nested more than 200 deep
// Two unguarded headers that include each other stop at the depth limit.
#include "include/pp_cycle_a.h"
kernel void pp_include_cycle_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
