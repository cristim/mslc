// EXPECT: valid
// Apple's AIR has no sampler state for a constexpr sampler nothing uses, and Iridium reads
// only that, so the sampler gets no binding and no embedded entry.
// REFLECT: "embedded_samplers": []
// REFLECT-NOT: "embedded_sampler": 0
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler unused(filter::linear);
  return t.read(uint2(0));
}
