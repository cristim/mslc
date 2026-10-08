// EXPECT: valid
// DISASM: OpImageSampleImplicitLod
// REFLECT: "embedded_sampler": 0, "name": "S"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; static float4 look(texture2d<float> t) { return t.sample(S, float2(0.25)); } };
fragment float4 f(texture2d<float> t [[texture(0)]]) { return Lookup::look(t); }
