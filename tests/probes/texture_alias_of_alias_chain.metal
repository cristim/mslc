// EXPECT: valid
// An alias of an alias of an alias still names t1.
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 1
// DISASM-MATCH-CAPTURED: OpLoad %[0-9]+ $1
// REFLECT-NOT: "binding": 4 }
// DISASM-NO-MATCH: Binding 4
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]])
{
  texture2d<float> b = t1;
  auto c = b;
  texture2d<float> d = c;
  return d.sample(s0, float2(0.5));
}
