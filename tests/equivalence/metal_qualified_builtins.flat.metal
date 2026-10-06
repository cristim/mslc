#include <metal_stdlib>
using namespace metal;
float4 twice(float4 v) { return v + v; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  float4 v = float4(float(i));
  out[i] = sin(v.x) + clamp(v.y, 0.0f, 1.0f) + twice(v).z + twice(v).w;
}
