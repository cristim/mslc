// EXPECT: valid
// C has A's state, so it shares A's variable; B keeps its own. B loads its own variable, then C loads A's.
// DISASM-MATCH: OpLoad %15 %20.*OpLoad %15 %17
#include <metal_stdlib>
using namespace metal;
constexpr sampler A(filter::linear);
constexpr sampler B(filter::nearest);
constexpr sampler C(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.sample(A, float2(0.25)) + t.sample(B, float2(0.5)) + t.sample(C, float2(0.75));
}
