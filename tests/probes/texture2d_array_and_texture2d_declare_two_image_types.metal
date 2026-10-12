// EXPECT: valid
// Arrayed is the only difference between texture2d and texture2d_array's image
// type; the per-kind image cache must not merge them.
// DISASM: OpTypeImage %float 2D 2 0 0 1 Unknown
// DISASM: OpTypeImage %float 2D 2 1 0 1 Unknown
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> a [[texture(0)]], texture2d_array<float> b [[texture(1)]],
    sampler s [[sampler(0)]]) {
  return a.sample(s, float2(0.5)) + b.sample(s, float2(0.5), 0u);
}
