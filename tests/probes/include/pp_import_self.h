#import "pp_import_self.h"
kernel void pp_import_self(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 7474; }
