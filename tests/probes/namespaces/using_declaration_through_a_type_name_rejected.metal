// EXPECT: error "metal::uint2" is not a namespace
// Apple: 'uint2' is not a class, namespace, or enumeration
#include <metal_stdlib>

using metal::uint2::x;
kernel void using_declaration_through_a_type_name_rejected(device int* out [[buffer(0)]])
{
    out[0] = 1;
}
