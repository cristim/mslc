// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
// REFLECT: "mag_filter": "Nearest"
// A file-scope sampler spelled through the alias, with nearest filtering.
#include <metal_stdlib>
namespace M = ::metal;
constant M::sampler sg(M::filter::nearest);
kernel void kern(device float* out [[buffer(0)]], M::texture2d<float> t [[texture(0)]]) { out[0] = t.sample(sg, float2(0.5)).x; }
