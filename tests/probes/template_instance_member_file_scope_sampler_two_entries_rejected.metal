// EXPECT: error only through argument-dependent lookup or a template instance
// Apple accepts this. The member look names S and is reached from a template instance, which the scan
// before emission does not follow; the instance is shared by two entry points, so it is refused.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct L { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x)); } };
template <typename T> float4 via(texture2d<float> t, T x) { L l; l.x = float(x); return l.look(t); }
fragment float4 f(texture2d<float> t [[texture(0)]]) { L l; l.x = 0.5; return l.look(t) + via(t, 0.25f); }
fragment float4 g(texture2d<float> u [[texture(1)]], texture2d<float> t [[texture(0)]]) { return via(t, 0.25f); }
