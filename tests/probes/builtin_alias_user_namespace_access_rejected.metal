// EXPECT: error expected an access qualifier
// U is a user namespace, so U::access::read is not the library's access qualifier.
#include <metal_stdlib>
namespace U { namespace access { constant int read = 1; } }
kernel void kern(metal::texture2d<float, U::access::read> t [[texture(0)]], device float* out [[buffer(0)]]) { out[0] = 1; }
