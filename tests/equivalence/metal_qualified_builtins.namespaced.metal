#include <metal_stdlib>
metal::float4 twice(metal::float4 v) { return v + v; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  metal::float4 v = metal::float4(float(i));
  out[i] = metal::sin(v.x) + metal::clamp(v.y, 0.0f, 1.0f) + twice(v).z + ::twice(v).w;
}
