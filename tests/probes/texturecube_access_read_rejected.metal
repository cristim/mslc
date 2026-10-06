// EXPECT: error texturecube access::read is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float, access::read> t [[texture(0)]]) {
  return float4(0.0);
}
