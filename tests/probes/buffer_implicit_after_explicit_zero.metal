// EXPECT: valid
// Apple: in takes the lowest free index, 1, because out claims 0 (air.location_index read from xcrun metal).
// REFLECT: "kind": "Buffer", "metal_index": 0, "descriptor": { "set": 0, "binding": 0 }, "member": 0, "param_index": 0, "name": "out"
// REFLECT: "kind": "Buffer", "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }, "member": 1, "param_index": 1, "name": "in"
#include <metal_stdlib>
using namespace metal;
kernel void k(device float *out [[buffer(0)]], device float *in, uint t [[thread_position_in_grid]]) { out[t] = in[t]; }
