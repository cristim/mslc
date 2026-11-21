// EXPECT: valid
// The gather component is the constant of the channel named: x 0, y 1, z 2, w 3, in call order (one ordered pattern, so swapping two constants fails).
// DISASM-MATCH: OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_1[^;]*OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_2[^;]*OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_3
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) {
  return a.gather(s, float2(0.5), int2(0), component::y) + a.gather(s, float2(0.5), int2(0), component::z) + a.gather(s, float2(0.5), int2(0), component::w);
}
