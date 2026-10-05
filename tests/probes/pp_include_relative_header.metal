// EXPECT: valid
// DISASM: OpConstant %int 4141
// A quoted include is found relative to the including file.
#include "include/pp_defs.h"
kernel void pp_include_relative_header(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = PP_HEADER_VALUE; }
