#import "include/equiv_defs.h"
#import "include/equiv_defs.h"
#include "include/equiv_defs.h"
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = EQUIV_VALUE; }
