// EXPECT: valid
// A constexpr sampler a helper declares and names is one of the calling entry point's embedded samplers, as one
// the entry point declares is: f has the coord::pixel one it declares and the two its helpers do (equal states
// share one), g only the one lin declares; an unnamed one records nothing.
// REFLECT: "kind": "Sampler", "descriptor": { "set": 1, "binding": 2 }, "embedded_sampler": 1, "name": "s"
// REFLECT: "kind": "Sampler", "descriptor": { "set": 1, "binding": 3 }, "embedded_sampler": 2, "name": "s"
// REFLECT: "mag_filter": "Linear", "min_filter": "Linear", "mip_filter": "None", "normalized_coordinates": false
// REFLECT: "s_address": "Repeat"
#include <metal_stdlib>
using namespace metal;
static float4 near(texture2d<float> t, float2 uv) { constexpr sampler s(mag_filter::nearest, min_filter::nearest); return t.sample(s, uv); }
static float4 lin(texture2d<float> t, float2 uv) { constexpr sampler s(mag_filter::linear, min_filter::linear, address::repeat); constexpr sampler unused(filter::linear); return t.sample(s, uv); }
static float4 both(texture2d<float> t, float2 uv) { constexpr sampler n2(mag_filter::nearest, min_filter::nearest); return t.sample(n2, uv) + lin(t, uv); }
fragment float4 f(texture2d<float> t [[texture(0)]], float4 p [[position]]) { constexpr sampler e(filter::linear, coord::pixel); return near(t, p.xy) + both(t, p.xy) + t.sample(e, p.xy); }
fragment float4 g(texture2d<float> t [[texture(0)]], float4 p [[position]]) { return lin(t, p.xy); }
