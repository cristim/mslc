// EXPECT: error wraps
// -sizeof(int) + 8 wraps to 4 in unsigned arithmetic, which Apple accepts and mslc does not model.
#include <metal_stdlib>
using namespace metal;

kernel void sizeof_negated_rejected(device float *out [[buffer(0)]])
{
    float a[-sizeof(int) + 8];
    out[0] = a[0];
}
