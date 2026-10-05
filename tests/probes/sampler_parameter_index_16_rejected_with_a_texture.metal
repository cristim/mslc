// EXPECT: error attribute "sampler" index 16 is out of bounds
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(16)]]) {
  return float4(0.0);
}
