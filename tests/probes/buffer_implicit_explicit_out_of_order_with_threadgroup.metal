// EXPECT: valid
// Apple: a=0, b=5, c=1, d=2, e=3; the threadgroup arguments number apart (tg=1, tg2=0).
// REFLECT: "metal_index": 0, "descriptor": { "set": 0, "binding": 0 }, "member": 0, "param_index": 0, "name": "a"
// REFLECT: "metal_index": 5, "descriptor": { "set": 0, "binding": 0 }, "member": 1, "param_index": 1, "name": "b"
// REFLECT: "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }, "member": 2, "param_index": 2, "name": "c"
// REFLECT: "metal_index": 2, "descriptor": { "set": 0, "binding": 0 }, "member": 3, "param_index": 3, "name": "d"
// REFLECT: "metal_index": 3, "descriptor": { "set": 0, "binding": 0 }, "member": 4, "param_index": 5, "name": "e"
#include <metal_stdlib>
using namespace metal;
kernel void k(device float *a, device float *b [[buffer(5)]], device float *c [[buffer(1)]], device float *d, threadgroup float *tg, constant float &e, threadgroup float *tg2 [[threadgroup(0)]], uint t [[thread_position_in_grid]]) { a[t] = b[t] + c[t] + d[t] + e + tg[0] + tg2[0]; }
