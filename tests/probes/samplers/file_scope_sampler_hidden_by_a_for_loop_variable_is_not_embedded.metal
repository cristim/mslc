// EXPECT: valid
// The condition and increment of a for loop name the loop's own S, not the file-scope sampler.
// REFLECT-NOT: "embedded_sampler"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f() {
  float4 r = float4(0.0);
  for (int S = 0; S < 2; ++S) {
    r += float4(1.0);
  }
  return r;
}
