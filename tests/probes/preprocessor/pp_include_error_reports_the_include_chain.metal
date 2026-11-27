// EXPECT: error included from 
// The chain of includes that led to it follows the error.
#include "include/pp_bad_directive.h"
kernel void pp_include_error_reports_the_include_chain(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
