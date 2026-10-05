// EXPECT: error the texture method "write" is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  t.write(float4(0.0), uint2(0));
  return float4(0.0);
}
