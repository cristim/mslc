// EXPECT: valid
// REFLECT: "texture_access": "Read"
// A namespace N::metal that the kernel's own scope does not see leaves the
// literal metal::access::read, and the unqualified access::read, the library's.
#include <metal_stdlib>
using namespace metal;
namespace N {
namespace metal { enum access { read = 7 }; }
}
kernel void texture_access_metal_qualifier_unshadowed_accepted(device float *out [[buffer(0)]],
        texture2d<float, metal::access::read> t [[texture(0)]])
{
    out[0] = t.read(uint2(0)).x;
}
