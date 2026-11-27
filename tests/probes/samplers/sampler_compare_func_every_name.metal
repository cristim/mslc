// EXPECT: valid
// Every compare_func name reaches the reflection under its own name, so none can be swapped for another.
// REFLECT: "compare_function": "Never"
// REFLECT: "compare_function": "Less", "anisotropy"
// REFLECT: "compare_function": "LessEqual"
// REFLECT: "compare_function": "Greater", "anisotropy"
// REFLECT: "compare_function": "GreaterEqual"
// REFLECT: "compare_function": "Equal"
// REFLECT: "compare_function": "NotEqual"
// REFLECT: "compare_function": "Always"
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(depth2d<float> d [[texture(0)]]) {
  constexpr sampler a(compare_func::never);
  constexpr sampler b(compare_func::less);
  constexpr sampler c(compare_func::less_equal);
  constexpr sampler e(compare_func::greater);
  constexpr sampler f(compare_func::greater_equal);
  constexpr sampler g(compare_func::equal);
  constexpr sampler h(compare_func::not_equal);
  constexpr sampler i(compare_func::always);
  float x = d.sample_compare(a, float2(0.5), 0.5) + d.sample_compare(b, float2(0.5), 0.5) + d.sample_compare(c, float2(0.5), 0.5)
    + d.sample_compare(e, float2(0.5), 0.5) + d.sample_compare(f, float2(0.5), 0.5) + d.sample_compare(g, float2(0.5), 0.5)
    + d.sample_compare(h, float2(0.5), 0.5) + d.sample_compare(i, float2(0.5), 0.5);
  return float4(x);
}
