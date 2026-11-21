// EXPECT: valid
// Real library names of every kind (function, type, enum, nested name) still
// bring in as before; Apple accepts these unused.
#include <metal_stdlib>
using metal::clamp;
using metal::float4;
using metal::texture2d;
using metal::access::read;
using metal::mem_flags;
using metal::simd_sum;
kernel void using_declaration_of_stdlib_names_accepted(device float* out [[buffer(0)]])
{
    out[0] = clamp(5.0f, 0.0f, 1.0f);
}
