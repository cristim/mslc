// EXPECT: valid
// DISASM: OpImageFetch
// REFLECT: "texture_access": "Read"
// M::access::read is the read access qualifier.
#include <metal_stdlib>
namespace M = ::metal;
kernel void kern(M::texture2d<float, M::access::read> t [[texture(0)]], device float* out [[buffer(0)]]) { out[0] = t.read(uint2(0)).x; }
