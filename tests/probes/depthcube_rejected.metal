// EXPECT: error "depthcube" is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(depthcube<float> t [[texture(0)]]) {
  return float4(0.0);
}
