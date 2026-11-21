// EXPECT: error no member named "notaname" in the namespace "metal::raytracing"
// Apple: no member named 'notaname' in namespace 'metal::raytracing'
#include <metal_stdlib>

using metal::raytracing::notaname;
kernel void using_declaration_of_misspelled_raytracing_name_rejected(device int* out [[buffer(0)]])
{
    out[0] = 1;
}
