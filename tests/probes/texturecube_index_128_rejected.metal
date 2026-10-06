// EXPECT: error attribute "texture" index 128 is out of bounds
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(128)]], sampler s [[sampler(0)]]) {
  return float4(0.0);
}
