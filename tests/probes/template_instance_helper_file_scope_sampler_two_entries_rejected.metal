// EXPECT: error reached by a path mslc does not follow
// Apple accepts this. look names S and is reached only through a template instance, which the scan before
// emission does not follow, so a and b would share one copy that binds one of their S variables.
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 look(texture2d<float> t, float2 uv) { return t.sample(S, uv); }
template <typename T> float4 via(texture2d<float> t, T x) { return look(t, float2(x)); }
fragment float4 a(texture2d<float> t [[texture(0)]]) { return via(t, 0.5f) + t.sample(S, float2(0.1)); }
fragment float4 b(texture2d<float> u [[texture(1)]], texture2d<float> t [[texture(0)]]) { return t.sample(S, float2(0.2)) + via(t, 0.25f); }
