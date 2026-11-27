// EXPECT: valid
//
// An auto helper that returns a texture's sample, as ANGLE's texture helpers are written, reached from a
// vertex and a fragment function, from inside a conditional and from a helper with a declared return type.
// Each stage has the deduced function once; Apple's compiler accepts the source.
#include <metal_stdlib>
using namespace metal;
struct V { float4 pos [[position]]; float2 uv; };
__attribute__((always_inline)) auto look(texture2d<float> t, sampler s, float2 uv) { return t.sample(s, uv); }
auto shared(float x) { float y = x + 1; return y * y; }
float plain(float x) { return shared(x) - 1; }
vertex V vs(uint id [[vertex_id]]) { V v; v.pos = float4(shared(float(id)), 0, 0, 1); v.uv = float2(0.5); return v; }
fragment float4 fs(V in [[stage_in]], texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  float c = in.uv.x > 0.2 ? shared(in.uv.y) : 2.0f;
  return look(t, s, in.uv) * c + plain(in.uv.x);
}
