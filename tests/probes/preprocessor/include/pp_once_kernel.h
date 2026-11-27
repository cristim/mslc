#pragma once
kernel void pp_once_kernel(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 6363; }
