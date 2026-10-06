// EXPECT: valid
// DISASM: "pp_import_skipped"
#if 0
#import "include/not_there.h"
#import PP_HEADER
#endif
kernel void pp_import_skipped(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1; }
