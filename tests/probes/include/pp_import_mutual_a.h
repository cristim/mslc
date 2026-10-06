#import "pp_import_mutual_b.h"
kernel void pp_import_mutual_a(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 7575; }
