// EXPECT: error is an absolute path
// An absolute path is outside every allowed root.
#include "/etc/hosts"
kernel void pp_include_absolute_path_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
