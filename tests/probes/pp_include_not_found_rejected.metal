// EXPECT: error #include "include/not_there.h" not found
// Replaces the "cannot honour" refusal of every quoted include: it is now searched for, and this is what is left.
#include "include/not_there.h"
kernel void pp_include_not_found_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
