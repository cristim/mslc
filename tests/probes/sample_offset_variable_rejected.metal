// EXPECT: error the offset of sample has to be written as an int2 of integer literals: Vulkan takes a constant texel offset
// Apple accepts a variable offset; Vulkan's sample takes a constant one.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], constant int2& q [[buffer(0)]]) {
  return a.sample(s, float2(0.25), q);
}
