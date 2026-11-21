// EXPECT: error subtracts below zero
// sizeof is unsigned, so 4 - 8 wraps; Apple rejects the array size too.
#include <metal_stdlib>
using namespace metal;

kernel void sizeof_array_length_unsigned_wrap_rejected(device float *out [[buffer(0)]])
{
    float a[sizeof(float) - 8];
    out[0] = a[0];
}
