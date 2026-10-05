// EXPECT: error #include with no file name
// No name at all.
#include
kernel void pp_include_without_a_name_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
