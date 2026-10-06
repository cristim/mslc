#include <metal_stdlib>
using namespace metal;
kernel void f(uint3 const i [[thread_position_in_grid]], uint3 const t [[thread_position_in_threadgroup]],
              texture2d<float> const tex [[texture(0)]], sampler const s [[sampler(0)]], device float4 *o [[buffer(0)]])
{ o[i.x + t.x] = tex.sample(s, float2(0.5)); }
