// EXPECT: valid
// Apple records no state for a sampler nothing names.
// REFLECT-NOT: "embedded_sampler"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f() {
  return float4(0.0);
}
