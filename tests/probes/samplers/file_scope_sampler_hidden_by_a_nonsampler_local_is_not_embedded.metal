// EXPECT: valid
// The only use of the name is the local float, so the program-scope sampler is unused.
// REFLECT-NOT: "embedded_sampler"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f() {
  float S = 1.0;
  return float4(S);
}
