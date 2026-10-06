// EXPECT: error the sampler option "compare_func" is not lowered yet
// A program-scope sampler takes the options a local one does, and refuses the same ones.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(compare_func::less);
fragment float4 f(texture2d<float> t [[texture(0)]]) { return t.sample(S, float2(0.25)); }
