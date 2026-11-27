// EXPECT: valid
// REFLECT: "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }
// REFLECT: "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }
// REFLECT: "kind": "InputAttachment", "metal_index": 0, "descriptor": { "set": 1, "binding": 2 }

#include <metal_stdlib>
using namespace metal;

// A constexpr sampler keeps the binding right after the textures; the attachment follows it.
fragment float4 framebuffer_fetch_after_constexpr_sampler(texture2d<float> tex [[texture(0)]],
    float4 dst [[color(0)]]) {
  constexpr sampler smp(filter::linear);
  return dst + tex.sample(smp, float2(0.5));
}
