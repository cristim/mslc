// EXPECT: valid
// An alias argument is the descriptor it names: the call passes t1's variable.
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 1
// DISASM-MATCH-CAPTURED: OpFunctionCall %v4float %[0-9]+ $1 %[0-9]+
// REFLECT-NOT: "binding": 4 }
// DISASM-NO-MATCH: Binding 4
#include <metal_stdlib>
using namespace metal;
float4 look(texture2d<float> t, sampler u) { return t.sample(u, float2(0.5)); }
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]])
{
  texture2d<float> b = t1;
  sampler u = s0;
  return look(b, u);
}
