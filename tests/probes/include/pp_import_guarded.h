#ifndef PP_IMPORT_GUARDED_H
#define PP_IMPORT_GUARDED_H
kernel void pp_import_guarded(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 7272; }
#endif
