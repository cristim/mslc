// EXPECT: valid
// DISASM: OpImageWrite
// REFLECT: "texture_access": "Write"
// M::access::write is the write access qualifier.
#include <metal_stdlib>
namespace M = ::metal;
kernel void kern(M::texture2d<float, M::access::write> t [[texture(0)]], device float* out [[buffer(0)]]) { t.write(float4(1.0), uint2(0)); out[0] = 1; }
