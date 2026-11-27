// EXPECT: valid
// DISASM-MATCH: OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_1[^;]*OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_2[^;]*OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_3[^;]*OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_0[^;]*OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %uint_2
#include <metal_stdlib>
using namespace metal;
typedef component comp_t;
constant constexpr component gc = component::z;
constant component gc2 = component::x;
fragment float4 hit(texture2d<float> a [[texture(0)]], texture2d_array<float> b [[texture(1)]],
    texturecube<float> c [[texture(2)]], texturecube_array<float> d [[texture(3)]], sampler s [[sampler(0)]]) {
  constexpr component cy = component::y; const component cz = component::z;
  constexpr metal::component mw = metal::component::w;
  using Comp = component;
  constexpr comp_t cx = comp_t::x; constexpr Comp czAlias = Comp::z; constexpr component copy = cy;
  constexpr component castC = static_cast<component>(2); constexpr component ctorC = component(3);
  constexpr bool flag = true; constexpr component tern = flag ? component::z : component::x;
  int gc = 0;
  int n = int(copy) + (cy == copy ? 1 : 0) + (czAlias == cz ? 1 : 0) + gc;
  float4 r = a.gather(s, float2(0.5), int2(0), cy) + a.gather(s, float2(0.5), int2(0), cz)
    + a.gather(s, float2(0.5), int2(0), mw) + b.gather(s, float2(0.5), 1u, int2(0), cx)
    + c.gather(s, float3(1, 0, 0), ::gc) + d.gather(s, float3(1, 0, 0), 1u, ctorC)
    + a.gather(s, float2(0.5), int2(0), castC) + a.gather(s, float2(0.5), int2(0), gc2)
    + a.gather(s, float2(0.5), int2(0), tern);
  return r + float(n);
}
