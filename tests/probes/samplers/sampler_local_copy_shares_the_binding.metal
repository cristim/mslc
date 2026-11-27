// EXPECT: valid
// Apple accepts a copy of a sampler parameter, and the copy is the same sampler: s1 is binding 3,
// the sample goes through its variable, and the copy gets no binding of its own.
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 3
// DISASM-MATCH-CAPTURED: OpLoad %[0-9]+ $1
// REFLECT-NOT: "binding": 4 }
// DISASM-NO-MATCH: Binding 4
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]])
{
  sampler x = s1;
  auto y = x;
  return t0.sample(y, float2(0.5));
}
