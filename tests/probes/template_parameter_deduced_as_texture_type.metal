// EXPECT: valid
// DISASM: OpImageQuerySize
// A type parameter taken by value from a texture or a sampler argument is that texture's or sampler's type: each
// call gets an instance for its own texture type, whose SPIR-V is that of the same functions written by hand.
#include <metal_stdlib>
using namespace metal;
template <typename Tex> static uint2 coords(Tex t, float2 uv) { uint2 d(t.get_width(), t.get_height()); return uint2(uv * float2(d)); }
template <typename Tex, typename S> static float4 smp(Tex t, S s, float2 uv) { return t.sample(s, uv); }
fragment float4 f(texture2d<float> a [[texture(0)]], texture2d_ms<float> m [[texture(1)]], texture2d<uint, access::read> u [[texture(2)]], sampler s [[sampler(0)]], float4 pos [[position]]) {
  uint2 c1 = coords(a, pos.xy);
  uint2 c2 = coords(m, pos.xy);
  uint2 c3 = coords(u, pos.xy);
  return m.read(c2, 0) + a.read(c1) + float4(u.read(c3)) + smp(a, s, pos.xy);
}
