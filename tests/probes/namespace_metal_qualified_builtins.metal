// EXPECT: valid
// DISASM: Sin
// DISASM: FClamp
//
// metal::name is the standard library's name, with or without "using namespace metal".
#include <metal_stdlib>
metal::float4 twice(metal::float4 v) { return v + v; }
kernel void kern(device float* out [[buffer(0)]])
{
  metal::float4 v = metal::float4(0.5);
  out[0] = metal::sin(v.x) + metal::clamp(v.y, 0.0f, 1.0f) + twice(v).z + ::twice(v).w;
}
