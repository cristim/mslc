_Pragma("once")
kernel void pp_pragma_operator_once_kernel(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 6364; }
