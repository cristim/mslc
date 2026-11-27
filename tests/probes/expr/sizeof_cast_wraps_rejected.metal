// EXPECT: error wraps
// (char)320 wraps to 64, which Apple accepts and mslc does not model.
#include <metal_stdlib>
using namespace metal;

kernel void sizeof_cast_wraps_rejected(device float *out [[buffer(0)]])
{
    float a[(char)sizeof(float4[20])];
    out[0] = a[0];
}
