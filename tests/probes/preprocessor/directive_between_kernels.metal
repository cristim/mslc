// EXPECT: valid
// The catch-all branch ends at the end of its own line. Skipping to the next
// '#', as mslc used to, consumed the kernel below.
#pragma once
#pragma pack(1)
kernel void directive_between_kernels(device uint* out [[buffer(0)]],
                                      uint i [[thread_position_in_grid]])
{ out[i] = i; }
