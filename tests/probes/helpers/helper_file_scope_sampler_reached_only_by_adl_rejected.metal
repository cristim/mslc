// EXPECT: error only through argument-dependent lookup
// Apple accepts this. The scan before emission does not follow argument-dependent lookup, so g is
// not known to depend on S and would share one body between a and b.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
namespace N { struct P { float x; };
  float4 d(texture2d<float> t, P p) { return t.sample(S, float2(p.x)); } }
float4 g(texture2d<float> t, N::P p) { return d(t, p); }
fragment float4 a(texture2d<float> t [[texture(0)]]) { N::P p; p.x = 0.25; return N::d(t, p) + g(t, p); }
fragment float4 b(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { N::P p; p.x = 0.75; return N::d(t, p) + g(t, p); }
