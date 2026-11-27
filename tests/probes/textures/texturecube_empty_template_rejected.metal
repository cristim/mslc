// EXPECT: error expected the sampled type of texturecube
// Apple: expected a type.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<> t [[texture(0)]]) {
  return float4(0.0);
}
