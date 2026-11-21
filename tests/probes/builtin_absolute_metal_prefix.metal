// EXPECT: valid
// DISASM: Sqrt %float_4
// REFLECT: "texture_access": "Sample"
// REFLECT: "mag_filter": "Linear"
// "::metal::" in a constexpr declaration, a texture type, its access and a call.
#include <metal_stdlib>
using namespace metal;
kernel void kern(device float* out [[buffer(0)]], ::metal::texture2d<float, ::metal::access::sample> t [[texture(0)]]) {
  constexpr ::metal::sampler s(::metal::filter::linear); out[0] = ::metal::sqrt(4.0f) + t.sample(s, float2(0.5)).x; }
