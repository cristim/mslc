// EXPECT: error a texture1d cannot be sampled with a coord::pixel sampler
// Apple accepts this; mslc lowers unnormalized coordinates on texture2d only, so this is a known false rejection.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture1d<float> t [[texture(0)]]) {
  constexpr sampler s(coord::pixel, filter::linear);
  return t.sample(s, 0.5f);
}
