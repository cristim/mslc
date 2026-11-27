// EXPECT: valid
// DISASM: OpIAdd
// include/lib/pp_inner.h includes "../pp_defs.h", which is relative to itself, not to this file.
#include "include/lib/pp_inner.h"
kernel void pp_include_nested_relative_to_includer(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = PP_INNER_VALUE; }
