// EXPECT: error a copy of another sampler is not lowered yet
// Apple accepts a copy of a sampler parameter.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(sampler s [[sampler(0)]]) {
  sampler x = s;
  return float4(0.0);
}
