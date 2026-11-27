// The vectors of vec_template_type.vec.metal written by name.
#include <metal_stdlib>
using namespace metal;
struct P { float2 a; int3 b; half4 c; uchar4 d; };
static float3 scaled(float3 v, float s) { return v * s; }
kernel void k(device const P* in [[buffer(0)]], device float4* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  float4 f = float4(in[i].a, 1.0f, 2.0f);
  uint2 u = uint2(i, i + 1);
  bool2 b = bool2(u.x > 3, u.y > 3);
  float3 s = scaled(float3(in[i].b), 0.5f);
  short2 h = short2(in[i].d.xy);
  out[i] = f + float4(float(u.y), b.y ? 1.0f : 0.0f, s.z, float(in[i].c.w) + float(h.y));
}
