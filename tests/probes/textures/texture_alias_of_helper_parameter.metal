// EXPECT: valid
// A helper's alias of its own parameter reads the parameter.
// REFLECT-NOT: "binding": 4 }
// DISASM-NO-MATCH: Binding 4
#include <metal_stdlib>
using namespace metal;
float4 look(texture2d<float> t, sampler u) { texture2d<float> b = t; sampler v = u; return b.sample(v, float2(0.5)); }
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]])
{
  return look(t1, s0) + look(t0, s1);
}
