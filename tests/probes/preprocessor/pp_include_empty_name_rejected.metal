// EXPECT: error #include with an empty file name
// An empty name.
#include ""
kernel void pp_include_empty_name_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
