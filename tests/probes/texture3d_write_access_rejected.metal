// EXPECT: error texture3d access::write is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture3d<float, access::write> t [[texture(0)]]) {
  return float4(0.0);
}
