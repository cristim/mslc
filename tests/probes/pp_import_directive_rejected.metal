// EXPECT: error unsupported preprocessor directive "#import"
// Apple accepts #import; mslc names it.
#import "include/pp_defs.h"
kernel void pp_import_directive_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
