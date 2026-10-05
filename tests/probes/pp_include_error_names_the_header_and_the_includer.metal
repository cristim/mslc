// EXPECT: error pp_bad_directive.h:2:2: unknown preprocessor directive
// An error inside a header is reported at the header's own file:line:col.
#include "include/pp_bad_directive.h"
kernel void pp_include_error_names_the_header_and_the_includer(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
