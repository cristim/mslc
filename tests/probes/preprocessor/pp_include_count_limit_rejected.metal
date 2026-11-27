// EXPECT: error #include directives in one translation
// 30 x 30 x 30 includes of a header that is not deep, so the depth limit is not what stops it.
#include "include/pp_fan_a.h"
kernel void pp_include_count_limit_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
