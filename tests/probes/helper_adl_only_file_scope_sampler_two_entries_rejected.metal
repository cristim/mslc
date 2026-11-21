// EXPECT: error reached by a path mslc does not follow
// Apple accepts this. d names S and is reached only by argument-dependent lookup, which the scan before
// emission does not follow, so a and b would share one copy that binds one of their S variables.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
namespace N { struct P { float x; };
  float4 d(texture2d<float> t, P p) { return t.sample(S, float2(p.x)); } }
fragment float4 a(texture2d<float> t [[texture(0)]]) { N::P p; p.x = 0.25; return d(t, p) + t.sample(S, float2(0.1)); }
fragment float4 b(texture2d<float> u [[texture(1)]], texture2d<float> t [[texture(0)]]) { N::P p; p.x = 0.75; return t.sample(S, float2(0.2)) + d(t, p); }
