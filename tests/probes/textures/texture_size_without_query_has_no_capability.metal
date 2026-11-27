// EXPECT: valid
// A shader that asks for no size does not declare ImageQuery.
// DISASM-NOT: ImageQuery
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25));
}
