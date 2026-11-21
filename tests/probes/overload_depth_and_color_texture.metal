// EXPECT: valid
// A depth2d and a texture2d parameter are different types, so the overloads coexist and each call picks its own.
// DISASM: OpFunctionCall
#include <metal_stdlib>
using namespace metal;
float g(depth2d<float> t, sampler s) { return t.sample(s, float2(0.5)); }
float g(texture2d<float> t, sampler s) { return t.sample(s, float2(0.5)).x; }
fragment float4 hit(depth2d<float> d [[texture(0)]], texture2d<float> c [[texture(1)]], sampler s [[sampler(0)]]) {
  return float4(g(d, s), g(c, s), 0, 1);
}
