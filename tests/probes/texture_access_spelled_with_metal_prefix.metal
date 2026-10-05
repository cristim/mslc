// EXPECT: valid
// metal::access::sample is accepted by Apple.
// DISASM: OpTypeImage %float 2D 2 0 0 1 Unknown
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float, metal::access::sample> t [[texture(0)]]) {
  return t.read(uint2(0));
}
