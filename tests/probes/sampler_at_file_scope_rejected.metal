// EXPECT: error "sampler" is a texture or sampler type
// Apple accepts a program-scope constexpr sampler, which Blender's blit shader uses.
// It is not lowered yet: declare the sampler inside the function.
#include <metal_stdlib>
using namespace metal;
constexpr sampler s(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.sample(s, float2(0.25));
}
