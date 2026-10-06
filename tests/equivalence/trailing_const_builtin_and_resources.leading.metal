#include <metal_stdlib>
using namespace metal;
kernel void f(const uint3 i [[thread_position_in_grid]], const uint3 t [[thread_position_in_threadgroup]],
              const texture2d<float> tex [[texture(0)]], const sampler s [[sampler(0)]], device float4 *o [[buffer(0)]])
{ o[i.x + t.x] = tex.sample(s, float2(0.5)); }
