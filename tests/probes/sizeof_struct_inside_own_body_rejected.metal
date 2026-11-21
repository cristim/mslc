// EXPECT: error inside its own body
// Apple rejects sizeof of an incomplete type.
#include <metal_stdlib>
using namespace metal;
struct Self { float a; float b[sizeof(Self)]; };
kernel void sizeof_struct_inside_own_body_rejected(device float *out [[buffer(0)]])
{
    Self s; out[0] = s.a;
}
