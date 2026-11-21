// EXPECT: valid
// h does not name S, it only calls a helper that does, and two fragment entry points share h: it is
// emitted per entry point too, or b would call a's copy and a's sampler.
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 2
// DISASM-MATCH-CAPTURED: OpLoad %[0-9]+ $1
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 d(texture2d<float> t) { return t.sample(S, float2(0.25)); }
float4 h(texture2d<float> t) { return d(t); }
fragment float4 a(texture2d<float> t [[texture(0)]]) { return h(t); }
fragment float4 b(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return h(t); }
