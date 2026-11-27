// EXPECT: error no member named "notalibraryname" in the namespace "metal"
// Apple: "no member named 'notalibraryname' in namespace 'N'". A namespace that
// reaches the library through a using directive has no such name to bring in.
#include <metal_stdlib>
using namespace metal;
namespace N { using namespace metal; }
using N::notalibraryname;
kernel void using_declaration_of_misspelled_stdlib_name_via_directive_rejected(device int* out [[buffer(0)]])
{
    out[0] = 1;
}
