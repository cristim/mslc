// EXPECT: valid
// compare_func is the embedded sampler's compare_function; the first option wins and an unset one stays Never.
// REFLECT: "normalized_coordinates": true, "compare_function": "LessEqual"
// REFLECT: "compare_function": "Never"
// DISASM: OpImageSampleDref
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(depth2d<float> a [[texture(0)]], texture2d<float> t [[texture(1)]]) {
  constexpr sampler s(compare_func::less_equal, compare_func::always, filter::linear);
  constexpr sampler plain(filter::nearest);
  return float4(a.sample_compare(s, float2(0.5), 0.5)) + t.sample(plain, float2(0.5));
}
