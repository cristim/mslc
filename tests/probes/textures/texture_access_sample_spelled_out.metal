// EXPECT: valid
// access::sample is the default access, and Apple accepts it spelled.
// DISASM: OpTypeImage %float 2D 2 0 0 1 Unknown
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float, access::sample> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25));
}
