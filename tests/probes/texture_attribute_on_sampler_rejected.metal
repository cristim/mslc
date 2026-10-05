// EXPECT: error its attribute is not valid for it
#include <metal_stdlib>
using namespace metal;
fragment float4 f(sampler s [[texture(0)]]) {
  return float4(0.0);
}
