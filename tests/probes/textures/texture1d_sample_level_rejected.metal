// EXPECT: error a texture1d samples with a sampler and a coordinate only
// Apple: sample(sampler, float) is the only sample overload, so level(0) is a no-match too.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture1d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return t.sample(s, 0.5f, level(0)); }
