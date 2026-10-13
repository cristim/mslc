// EXPECT: valid
// DISASM-MATCH: OpImageSampleImplicitLod %v4float
// DISASM: OpCapability Sampled1D
// REFLECT: { "kind": "Texture", "metal_index": 2, "descriptor": { "set": 1, "binding": 0 }, "texture_access": "Sample"
//
// A texture1d passes to a helper like a texture2d does (the corpus shape of a LUT lookup).
#include <metal_stdlib>
using namespace metal;
constexpr sampler lut(coord::normalized, address::clamp_to_edge, filter::linear);
half lookup(half x, texture1d<float, access::sample> t) { return half(t.sample(lut, float(x)).r); }
fragment half4 f(texture1d<float> t [[texture(2)]]) { return half4(lookup(0.25h, t)); }
