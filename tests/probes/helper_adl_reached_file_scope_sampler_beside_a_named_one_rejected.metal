// EXPECT: error reached by a path mslc does not follow
// Apple accepts this. g reaches d, which names T, only by argument-dependent lookup, so the scan before
// emission does not mark d and a and b would share one copy that binds one of their T variables.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
constexpr sampler T(filter::nearest);
namespace N { struct P { float x; };
  float4 d(texture2d<float> t, P p) { return t.sample(T, float2(p.x)); } }
float4 g(texture2d<float> t, N::P p) { return d(t, p) + t.sample(S, float2(0.1)); }
fragment float4 a(texture2d<float> u [[texture(1)]], texture2d<float> t [[texture(0)]]) { N::P p; p.x = 0.25; return g(t, p) + t.sample(T, float2(0.3)); }
fragment float4 b(texture2d<float> t [[texture(0)]]) { N::P p; p.x = 0.25; return t.sample(T, float2(0.3)) + g(t, p); }
