// EXPECT: error is a texturecube<float>, and its attribute is not valid for it
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[buffer(0)]]) {
  return float4(0.0);
}
