// EXPECT: error texture2d access::write is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float, access::write> t [[texture(0)]]) {
  return float4(0.0);
}
