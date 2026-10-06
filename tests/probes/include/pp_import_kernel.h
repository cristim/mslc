kernel void pp_import_kernel(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 7171; }
