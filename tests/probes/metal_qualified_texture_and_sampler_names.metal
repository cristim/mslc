// EXPECT: valid
// metal::texture2d and metal::sampler are Apple's spellings of the unqualified names.
// DISASM: OpTypeImage %float 2D 2 0 0 1 Unknown
#include <metal_stdlib>
using namespace metal;
fragment float4 f(metal::texture2d<float> t [[texture(0)]], metal::sampler s [[sampler(0)]])
{ return t.sample(s, float2(0.25)); }
