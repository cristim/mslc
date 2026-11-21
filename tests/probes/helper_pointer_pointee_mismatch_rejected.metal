// EXPECT: error argument 1 of the call to "f" is a buffer of packed_float3, and parameter "p" points at float3*, which is not the same element type
#include <metal_stdlib>
using namespace metal;
float f(device float3* p, uint i) { return p[i].x; }
kernel void helper_pointer_pointee_mismatch_rejected(device float* out [[buffer(0)]],
                                                     device packed_float3* in [[buffer(1)]],
                                                     uint i [[thread_position_in_grid]])
{
    out[i] = f(in, i);
}
