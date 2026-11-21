// EXPECT: error is initialised from "sampler(options)" or a brace list only
// Apple accepts a file-scope copy of a sampler.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
constexpr sampler U = S;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]], constant int& c [[buffer(0)]]) {
  return float4(0.0);
}
