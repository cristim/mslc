// EXPECT: valid
// DISASM: OpConstant %int 5959
// DISASM: OpConstant %int 6060
// __INCLUDE_LEVEL__ is 0 in the source and 1 in a header it includes.
#include "include/pp_level.h"
#if __INCLUDE_LEVEL__ == 0
#define PP_LEVEL_HERE 6060
#endif
kernel void pp_include_level(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = PP_LEVEL_IN_HEADER + PP_LEVEL_HERE; }
