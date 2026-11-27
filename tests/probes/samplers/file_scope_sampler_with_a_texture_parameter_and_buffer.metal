// EXPECT: valid
// The embedded sampler takes the binding after the texture, as a local one does.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 2 }, "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]], device float* b [[buffer(0)]]) {
  return t.sample(S, float2(b[0]));
}
