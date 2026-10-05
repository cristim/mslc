// EXPECT: error "texturecube" is not lowered yet
// Left for the cube map change.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]]) {
  return float4(0.0);
}
