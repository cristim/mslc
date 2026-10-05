// EXPECT: error is not a regular file
// A directory is not a header.
#include "include"
kernel void pp_include_of_a_directory_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
