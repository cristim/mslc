// EXPECT: valid
// QUIRK (helper parameter attributes): on a helper, [[base_vertex]] on a ushort is accepted and has no effect;
// the argument is what the call passes.
#include <metal_stdlib>
using namespace metal;
float f(ushort b [[base_vertex]]) { return float(b); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(ushort(3)); }
