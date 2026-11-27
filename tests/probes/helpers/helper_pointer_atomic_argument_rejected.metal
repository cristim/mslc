// EXPECT: error argument 1 of the call to "read" is a buffer of atomic_uint, and parameter "p" points at uint*, which is not the same element type
#include <metal_stdlib>
using namespace metal;
float read(device uint* p, uint i) { return (float)p[i]; }
kernel void helper_pointer_atomic_argument_rejected(device float* out [[buffer(0)]],
                                                    device atomic_uint* counters [[buffer(1)]],
                                                    uint i [[thread_position_in_grid]])
{
    out[i] = read(counters, i);
}
