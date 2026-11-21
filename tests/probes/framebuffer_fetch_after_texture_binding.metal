// EXPECT: valid
// REFLECT: "kind": "Texture", "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }
// REFLECT: "kind": "Sampler", "metal_index": 0, "descriptor": { "set": 1, "binding": 1 }
// REFLECT: "kind": "InputAttachment", "metal_index": 0, "descriptor": { "set": 1, "binding": 2 }

#include <metal_stdlib>
using namespace metal;

fragment float4 framebuffer_fetch_after_texture_binding(texture2d<float> tex [[texture(0)]],
    sampler smp [[sampler(0)]], float4 dst [[color(0)]]) {
  return dst + tex.sample(smp, float2(0.5));
}
