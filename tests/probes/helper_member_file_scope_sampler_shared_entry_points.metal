// EXPECT: valid
// Two fragment entry points share one member that names S. Each has its own copy, naming its own
// sampler variable, so the sampler is in each entry point's interface.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 2 }, "embedded_sampler": 0, "name": "S" }
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 1
// DISASM-CAPTURE: OpDecorate (%[0-9]+) Binding 2
// DISASM-MATCH-CAPTURED: OpLoad %[0-9]+ $1
// DISASM-MATCH-CAPTURED: OpLoad %[0-9]+ $2
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x)); } };
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  Lookup l; l.x = 0.25; return l.look(t) + t.sample(S, float2(0.5));
}
fragment float4 g(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  Lookup l; l.x = 0.75; return l.look(t) + t.sample(S, float2(0.5));
}
