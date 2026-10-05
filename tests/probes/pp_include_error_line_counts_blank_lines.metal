// EXPECT: error pp_error_after_blank_lines.h:4:2: #error directive in this source: from the header
// Line 4 of the header, though the first three are blank.
#include "include/pp_error_after_blank_lines.h"
kernel void pp_include_error_line_counts_blank_lines(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
