// EXPECT: error no member named "sqrtt" in the namespace "metal::precise"
// Apple: no member named 'sqrtt' in namespace 'metal::precise'
#include <metal_stdlib>

using metal::precise::sqrtt;
kernel void using_declaration_of_misspelled_precise_name_rejected(device int* out [[buffer(0)]])
{
    out[0] = 1;
}
