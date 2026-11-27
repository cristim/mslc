// EXPECT: error is a sampler, which mslc uses as the receiver of a texture call
#include <metal_stdlib>
using namespace metal;
fragment float4 f(sampler s [[sampler(0)]]) {
  float x = s;
  return float4(x);
}
