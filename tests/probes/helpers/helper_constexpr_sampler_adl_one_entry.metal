// EXPECT: valid
// A helper that declares a constexpr sampler, reached through argument-dependent lookup: with one entry point
// no body is shared between entry points, so it is lowered.
#include <metal_stdlib>
using namespace metal;
static float4 H(texture2d<float> t, float2 uv) { constexpr sampler s(filter::linear); return t.sample(s, uv); }
namespace N { struct W { float k; }; float4 hs(W w, texture2d<float> t, float2 uv) { return H(t, uv) * w.k; } }
fragment float4 f(texture2d<float> t [[texture(0)]], float4 p [[position]]) { N::W w{1}; constexpr sampler e(filter::linear); return t.sample(e, p.xy) + H(t, p.xy) + hs(w, t, p.xy); }
