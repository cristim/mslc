// EXPECT: error pp_unterminated_if.h:1:1: unterminated conditional directive
// The error is reported where the #if is, in the header, with the include that led there.
#include "include/pp_unterminated_if.h"
kernel void pp_unterminated_if_in_a_header_names_the_header(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
