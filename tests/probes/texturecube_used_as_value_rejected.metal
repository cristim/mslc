// EXPECT: error is a texturecube<float>, which mslc uses as the receiver of a texture call
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]]) {
  return float4(t);
}
