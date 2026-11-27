// EXPECT: valid
// Apple: a=1, b=0, c=3, d=2, e=4 (each unattributed buffer takes the lowest index no attribute claims).
// REFLECT: "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }, "member": 0, "param_index": 0, "name": "a"
// REFLECT: "metal_index": 0, "descriptor": { "set": 0, "binding": 0 }, "member": 1, "param_index": 1, "name": "b"
// REFLECT: "metal_index": 3, "descriptor": { "set": 0, "binding": 0 }, "member": 2, "param_index": 2, "name": "c"
// REFLECT: "metal_index": 2, "descriptor": { "set": 0, "binding": 0 }, "member": 3, "param_index": 3, "name": "d"
// REFLECT: "metal_index": 4, "descriptor": { "set": 0, "binding": 0 }, "member": 4, "param_index": 4, "name": "e"
#include <metal_stdlib>
using namespace metal;
kernel void k(device float *a, device float *b [[buffer(0)]], device float *c, device float *d [[buffer(2)]], constant float &e, uint t [[thread_position_in_grid]]) { a[t] = b[t] + c[t] + d[t] + e; }
