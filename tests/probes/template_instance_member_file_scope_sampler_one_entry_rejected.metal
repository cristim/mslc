// EXPECT: error "S" is not a sampler
// Apple accepts this. The member look names S and is reached only from a template instance, so the sampler
// is never bound for the entry point; refused, never a module that binds the wrong sampler.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct L { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x)); } };
template <typename T> float4 via(texture2d<float> t, T x) { L l; l.x = float(x); return l.look(t); }
fragment float4 g(texture2d<float> u [[texture(1)]], texture2d<float> t [[texture(0)]]) { return via(t, 0.25f); }
