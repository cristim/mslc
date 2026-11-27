// EXPECT: error sizeof of void
// Apple rejects sizeof(void) too.
#include <metal_stdlib>
using namespace metal;

kernel void sizeof_void_rejected(device float *out [[buffer(0)]])
{
    out[0] = sizeof(void);
}
