// EXPECT: valid
// depth2d<T> deduces T = float from a depth2d<float>.
#include <metal_stdlib>
using namespace metal;
template <typename T> float g(depth2d<T> a, sampler s) { return a.sample(s, float2(0)); }
fragment float4 f(depth2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return float4(g(t, s)); }
