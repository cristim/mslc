// EXPECT: valid
// REFLECT: "binding": 1 }, "embedded_sampler": 0, "name": "F1"
// REFLECT: "binding": 2 }, "embedded_sampler": 1, "name": "F2"
// The file-scope samplers keep the bindings they get without helpers: a helper's own constexpr sampler is
// reserved after them.
#include <metal_stdlib>
using namespace metal;
constexpr sampler F1(address::repeat);
constexpr sampler F2(filter::linear);
static float4 h(texture2d<float> t, float2 uv) { constexpr sampler s(filter::linear); return t.sample(s, uv); }
fragment float4 f(texture2d<float> t [[texture(0)]], float4 p [[position]]) { return t.sample(F1, p.xy) + t.sample(F2, p.xy) + h(t, p.xy); }
