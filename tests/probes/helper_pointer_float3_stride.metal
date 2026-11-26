// EXPECT: valid
// A float3 buffer steps 16 bytes in Metal while the SPIR-V vector is 12: the
// helper's pointer arithmetic is byte-exact, so trip + 1 and p[i].x reach the
// element Metal would name, not the byte Metal would not.
#include <metal_stdlib>
using namespace metal;
float readTriple(device float3* p, uint i) { return p[i].x; }
kernel void helper_pointer_float3_stride(device float* out [[buffer(0)]],
                                         device float3* trip [[buffer(1)]],
                                         uint i [[thread_position_in_grid]])
{
    out[i] = readTriple(trip + 1, i);
}
