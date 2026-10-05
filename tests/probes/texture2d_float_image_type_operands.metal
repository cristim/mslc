// EXPECT: valid
// A texture2d<float> is a separate OpTypeImage with a float sampled type, 2D, depth
// unspecified, not arrayed, single sampled, Sampled 1 and format Unknown, as Iridium
// declares it (indium src/iridium/air.cpp:771). The format is Unknown because the
// module does not know what format the app binds; spirv-val accepts any format, so
// only this operand check can pin it.
// DISASM: OpTypeImage %float 2D 2 0 0 1 Unknown
// DISASM: OpTypeSampler
// DISASM-NOT: OpTypeImage %half
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25));
}
