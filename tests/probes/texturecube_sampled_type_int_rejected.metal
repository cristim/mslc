// EXPECT: error texturecube<int> is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<int> t [[texture(0)]]) {
  return float4(0.0);
}
