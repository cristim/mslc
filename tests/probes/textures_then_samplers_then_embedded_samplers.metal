// EXPECT: valid
// Iridium's order: textures, then sampler parameters, then constexpr samplers, whatever
// order the parameters are declared in (air.cpp:695, 790, 844). Here the parameters are
// sampler, texture, sampler, texture and the bindings are texture 0, 1, sampler 2, 3 and
// the constexpr sampler 4.
// REFLECT: "metal_index": 5, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample", "param_index": 1, "name": "ta"
// REFLECT: "metal_index": 6, "descriptor": { "set": 1, "binding": 1 }, "texture_access": "Sample", "param_index": 3, "name": "tb"
// REFLECT: { "kind": "Sampler", "metal_index": 2, "descriptor": { "set": 1, "binding": 2 }, "param_index": 0, "name": "sa" }
// REFLECT: { "kind": "Sampler", "metal_index": 3, "descriptor": { "set": 1, "binding": 3 }, "param_index": 2, "name": "sb" }
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 4 }, "embedded_sampler": 0, "name": "e" }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(sampler sa [[sampler(2)]], texture2d<float> ta [[texture(5)]], sampler sb [[sampler(3)]], texture2d<float> tb [[texture(6)]]) {
  constexpr sampler e(filter::linear);
  return ta.sample(sa, float2(0.25)) + tb.sample(sb, float2(0.25)) + ta.sample(e, float2(0.25));
}
