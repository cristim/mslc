// EXPECT: error no member named "notalibraryname" in the namespace "metal"
// Apple: "no member named 'notalibraryname' in namespace 'metal'", at file scope
// and inside a namespace alike, even when nothing uses the name.
#include <metal_stdlib>
namespace N { using metal::notalibraryname; }
kernel void using_declaration_of_misspelled_stdlib_name_rejected(device int* out [[buffer(0)]])
{
    out[0] = 1;
}
