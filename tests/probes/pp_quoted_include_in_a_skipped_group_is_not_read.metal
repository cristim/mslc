// EXPECT: valid
// DISASM: OpConstant %int 5656
// A skipped #include is neither searched for nor diagnosed.
#if 0
#include "include/not_there.h"
#include <not_a_header>
#endif
kernel void pp_quoted_include_in_a_skipped_group_is_not_read(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 5656; }
