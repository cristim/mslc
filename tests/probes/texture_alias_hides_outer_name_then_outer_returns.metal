// EXPECT: valid
// The inner "b" names t0 inside its block and the outer "b" names t1 again after it: the loads are
// in that order.
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 0
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 1
// DISASM-MATCH-CAPTURED: OpLoad %[0-9]+ $1.*OpLoad %[0-9]+ $2
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]])
{
  texture2d<float> b = t1;
  float4 r = float4(0.0);
  {
    texture2d<float> b = t0;
    r = b.sample(s0, float2(0.5));
  }
  return r + b.sample(s0, float2(0.25));
}
