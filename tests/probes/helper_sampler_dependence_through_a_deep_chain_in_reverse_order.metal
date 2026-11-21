// EXPECT: valid
// Two chains end in a function that names S. One writes its bodies callee first, the other caller
// first behind prototypes, so a single pass over the call graph, in definition order or against it,
// meets some caller before the callee that makes it sampler-dependent. Every link needs its own copy
// per entry point; one pass marks only the links above S in that pass's own direction.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 f3(texture2d<float> t);
float4 f2(texture2d<float> t);
float4 f1(texture2d<float> t);
float4 f0(texture2d<float> t);
float4 g0(texture2d<float> t) { return t.sample(S, float2(0.25)); }
float4 g1(texture2d<float> t) { return g0(t); }
float4 g2(texture2d<float> t) { return g1(t); }
float4 g3(texture2d<float> t) { return g2(t); }
float4 f3(texture2d<float> t) { return f2(t); }
float4 f2(texture2d<float> t) { return f1(t); }
float4 f1(texture2d<float> t) { return f0(t); }
float4 f0(texture2d<float> t) { return t.sample(S, float2(0.75)); }
struct M { float x; float4 m(texture2d<float> t) const { return f3(t) + g3(t); } };
fragment float4 a(texture2d<float> t [[texture(0)]]) { M m; m.x = 0; return m.m(t); }
fragment float4 b(texture2d<float> u [[texture(1)]], texture2d<float> t [[texture(0)]]) { M m; m.x = 0; return m.m(t); }
