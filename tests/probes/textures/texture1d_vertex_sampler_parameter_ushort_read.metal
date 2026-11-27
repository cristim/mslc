// EXPECT: valid
// DISASM: OpImageSampleExplicitLod %v4float
// DISASM: OpUConvert %uint
// DISASM: OpConvertUToF %float
//
// A vertex function samples a texture1d through a sampler parameter with an unsigned
// coordinate and reads it with a ushort coordinate; both compile on Apple.
#include <metal_stdlib>
using namespace metal;
vertex float4 v(texture1d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, 1u) + t.read(ushort(1));
}
