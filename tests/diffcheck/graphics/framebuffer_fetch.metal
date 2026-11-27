// RUN: vertex=vs fragment=fs cases=2
// DIFFCHECK-POSITION: pos
#include <metal_stdlib>
using namespace metal;

struct VIn {
  float2 pos [[attribute(0)]];
  float2 uv [[attribute(1)]];
};

struct VOut {
  float4 pos [[position]];
  float2 uv [[flat]];
};

struct FIn {
  float4 prev0 [[color(0)]];
  float2 uv [[flat]];
  float4 pos [[position]];
};

struct FOut {
  float4 f [[color(0)]];
  half4 h [[color(2)]];
  int4 i [[color(3)]];
  uint4 u [[color(7)]];
};

vertex VOut vs(VIn in [[stage_in]]) {
  VOut out;
  out.pos = float4(in.pos, 0.5f, 1.0f);
  out.uv = in.uv;
  return out;
}

fragment FOut fs(FIn in [[stage_in]], texture2d<float> tex [[texture(0)]],
    uint4 u [[color(7)]], half4 h [[color(2)]], int4 i [[color(3)]]) {
  constexpr sampler smp(filter::nearest);
  float4 s = tex.sample(smp, float2(0.125f, 0.375f));
  FOut out;
  out.f = float4(1.0f - in.prev0.rgb, in.prev0.a) + s + float4(in.uv, 0.0f, 0.0f);
  out.h = h + half4(1.0h, 2.0h, 3.0h, 4.0h);
  out.i = i + int4(1, 2, 3, 4);
  out.u = u + uint4(5u, 6u, 7u, 8u);
  return out;
}
