// EXPECT: error no 'buffer' resource location is available for parameter "p31"
// Apple: no 'buffer' resource location available for 'p31' (explicit buffer(0) leaves 1 to 30 for 31 unattributed buffers).
#include <metal_stdlib>
using namespace metal;
kernel void k(device float *p0 [[buffer(0)]], device float *p1, device float *p2, device float *p3, device float *p4, device float *p5, device float *p6, device float *p7, device float *p8, device float *p9, device float *p10, device float *p11, device float *p12, device float *p13, device float *p14, device float *p15, device float *p16, device float *p17, device float *p18, device float *p19, device float *p20, device float *p21, device float *p22, device float *p23, device float *p24, device float *p25, device float *p26, device float *p27, device float *p28, device float *p29, device float *p30, device float *p31, uint t [[thread_position_in_grid]]) { p0[t] = 1; }
