// EXPECT: valid
// DISASM-MATCH: OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_1[^;]*OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_3
#include <metal_stdlib>
using metal::component;
kernel void hit(metal::texture2d<float> a [[texture(0)]], metal::sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  constexpr component cy = component::y; constexpr metal::component mw = metal::component::w;
  o[0] = a.gather(s, metal::float2(0.5), metal::int2(0), cy) + a.gather(s, metal::float2(0.5), metal::int2(0), mw);
}
