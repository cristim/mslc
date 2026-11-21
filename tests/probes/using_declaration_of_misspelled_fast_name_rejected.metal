// EXPECT: error no member named "notaname" in the namespace "metal::fast"
// Apple: no member named 'notaname' in namespace 'metal::fast'
#include <metal_stdlib>

using metal::fast::notaname;
kernel void using_declaration_of_misspelled_fast_name_rejected(device int* out [[buffer(0)]])
{
    out[0] = 1;
}
