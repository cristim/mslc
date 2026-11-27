// EXPECT: error not the library's metal::access
// Apple: "value of type 'N::metal::access' is not implicitly convertible to
// 'metal::access'". The nearer namespace metal supplies the enum, so the
// literal metal::access::read is not the library's access qualifier.
#include <metal_stdlib>
using namespace metal;
namespace N {
namespace metal { enum access { read = 7 }; }
kernel void texture_access_metal_qualifier_shadowed_by_user_namespace_rejected(device float *out [[buffer(0)]],
        texture2d<float, metal::access::read> t [[texture(0)]])
{
    out[0] = t.read(uint2(0)).x;
}
}
