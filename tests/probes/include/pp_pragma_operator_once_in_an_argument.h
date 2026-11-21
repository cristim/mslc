#define PP_ID(x) x
PP_ID(_Pragma("once"))
kernel void pp_pragma_operator_once_arg_kernel(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 6365; }
