// EXPECT: error a header named by a macro is not supported
// Computed includes are named, not guessed.
#define HEADER "include/pp_defs.h"
#include HEADER
kernel void pp_include_by_macro_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
