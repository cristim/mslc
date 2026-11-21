// EXPECT: error no member named "notamember" in the namespace "metal::access"
// Apple: no member named 'notamember' in 'metal::access'
#include <metal_stdlib>

using metal::access::notamember;
kernel void using_declaration_of_misspelled_access_member_rejected(device int* out [[buffer(0)]])
{
    out[0] = 1;
}
