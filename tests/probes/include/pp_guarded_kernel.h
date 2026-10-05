#ifndef PP_GUARDED_KERNEL_H
#define PP_GUARDED_KERNEL_H
kernel void pp_guarded_kernel(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 6262; }
#endif
