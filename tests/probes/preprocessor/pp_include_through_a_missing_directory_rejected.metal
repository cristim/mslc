// EXPECT: error #include "no_such_directory/../include/pp_defs.h" not found
// x/../a.h is not a.h unless x exists, so the file system is asked about the path as written.
#include "no_such_directory/../include/pp_defs.h"
kernel void pp_include_through_a_missing_directory_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
