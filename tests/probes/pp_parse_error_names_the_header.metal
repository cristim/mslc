// EXPECT: error pp_parse_error.h:1:1: unexpected
// A parse error is placed in the file the token was written in, not in the source that included it.
#include "include/pp_parse_error.h"
kernel void pp_parse_error_names_the_header(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
