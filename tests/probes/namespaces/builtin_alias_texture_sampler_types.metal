// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
// REFLECT: "texture_access": "Sample"
// REFLECT: "mag_filter": "Linear"
// REFLECT: "Sampler"
// M::texture2d, M::access::sample, M::sampler, M::filter::linear and M::address::clamp_to_edge
// are the library's: the texture, the sampler binding and the embedded sampler's state match
// the metal:: spelling.
#include <metal_stdlib>
namespace M = metal;
kernel void kern(M::texture2d<float, M::access::sample> t [[texture(0)]], M::sampler s [[sampler(0)]], device float* out [[buffer(0)]]) { constexpr M::sampler cs(M::filter::linear, M::address::clamp_to_edge); out[0] = t.sample(cs, float2(0.5)).x; }
