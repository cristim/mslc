// EXPECT: error sizeof of this expression is not lowered
// Apple rejects f2.z: the vector has two components. mslc refuses it too.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_swizzle_past_width_rejected(device ulong *out [[buffer(0)]])
{
    float2 f = 0;
    out[0] = sizeof(f.z + 1.0f);
}
