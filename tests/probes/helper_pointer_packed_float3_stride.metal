// EXPECT: valid
// A packed_float3 buffer steps 12 bytes, which is also the SPIR-V vector's
// size, so the byte-exact element step agrees with the tighter layout.
#include <metal_stdlib>
using namespace metal;
float readPacked(device packed_float3* p, uint i) { return p[i].y; }
kernel void helper_pointer_packed_float3_stride(device float* out [[buffer(0)]],
                                                device packed_float3* trip [[buffer(1)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = readPacked(trip + 2, i);
}
