// EXPECT: valid
// DISASM-MATCH: OpTypeImage %float 2D 2 0 0 1 Unknown
// DISASM-MATCH: OpTypeImage %uint 2D 0 0 0 2 Unknown
// REFLECT: "metal_index": 8, "descriptor": { "set": 1, "binding": 1 }, "texture_access": "Sample"
// REFLECT: "metal_index": 3, "descriptor": { "set": 1, "binding": 2 }, "texture_access": "Write"
// REFLECT: "kind": "Sampler", "metal_index": 4, "descriptor": { "set": 1, "binding": 3 }
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(device uint* out [[buffer(2)]], texture2d<float> sampled [[texture(8)]], texture2d<uint, access::write> dst [[texture(3)]], sampler s [[sampler(4)]]) {
  dst.write(0xffffffffu, uint2(0)); out[0] = dst.get_width();
  return sampled.sample(s, float2(0.25));
}
