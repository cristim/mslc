// EXPECT: error invalid values combination in sampler initialization
// Apple: "if 'coord::pixel' is specified the min_filter and mag_filter values must be the same, the mip_filter and compare_func values must be 'none', and address modes must be either 'clamp_to_zero', 'clamp_to_edge', or 'clamp_to_border'".
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(coord::pixel, mip_filter::linear);
  return t.sample(s, float2(0.25));
}
