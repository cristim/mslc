// EXPECT: valid
// REFLECT: "texture_access": "Read"
// A file that reopens the library's namespace metal (with an enum, or a nested namespace)
// has not shadowed it: metal::access::read is still the library's. Apple accepts both.
#include <metal_stdlib>
using namespace metal;
namespace metal { enum myacc { q }; namespace x {} }
kernel void texture_access_metal_qualifier_with_reopened_namespace_accepted(device float *out [[buffer(0)]],
        texture2d<float, metal::access::read> t [[texture(0)]])
{
    out[0] = t.read(uint2(0)).x;
}
