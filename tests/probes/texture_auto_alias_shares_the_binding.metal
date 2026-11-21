// EXPECT: valid
// "auto" bound to a texture is the same alias, and "const auto" and "thread" spell it too.
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 1
// DISASM-MATCH-CAPTURED: OpLoad %[0-9]+ $1
// REFLECT-NOT: "binding": 4 }
// DISASM-NO-MATCH: Binding 4
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]])
{
  const auto b = t1;
  thread texture2d<float> c = t0;
  return b.sample(s0, float2(0.5)) + c.get_width();
}
