// EXPECT: valid
// DISASM: NClamp
// REFLECT: "texture_access": "Sample"
// REFLECT: "mag_filter": "Linear"
// REFLECT: "normalized_coordinates": true
// A prefix through a namespace's alias (N::M) works in every position: texture and access
// in a parameter, sampler and options in a constexpr sampler, and a call.
#include <metal_stdlib>
namespace N { namespace M = ::metal; }
kernel void kern(device float* out [[buffer(0)]], N::M::texture2d<float, N::M::access::sample> t [[texture(0)]]) {
  constexpr N::M::sampler s(N::M::filter::linear, N::M::address::clamp_to_edge, N::M::coord::normalized);
  out[0] = N::M::clamp(2.0f, 0.0f, 1.0f) + t.sample(s, float2(0.5)).x; }
