// EXPECT: valid
// An instance member that names a file-scope sampler reserves it for the entry point that calls it.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
// DISASM: OpImageSampleImplicitLod
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x)); } };
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  Lookup l; l.x = 0.25; return l.look(t);
}
