// EXPECT: valid
// A texture local is another name for the texture it is initialised from, so a read through it
// loads t1's own variable (binding 1) and no variable of its own is declared.
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 1
// DISASM-MATCH-CAPTURED: OpLoad %[0-9]+ $1
// REFLECT-NOT: "binding": 4 }
// DISASM-NO-MATCH: Binding 4
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]])
{
  texture2d<float> b = t1;
  return b.sample(s0, float2(0.5));
}
