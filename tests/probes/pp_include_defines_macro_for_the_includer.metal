// EXPECT: valid
// DISASM: OpConstant %int 6565
// A macro defined in a header is visible after the include.
#include "include/pp_defines_macro.h"
kernel void pp_include_defines_macro_for_the_includer(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = FROM_HEADER; }
