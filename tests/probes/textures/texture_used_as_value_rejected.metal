// EXPECT: error is a texture2d<float>, which mslc uses as the receiver of a texture call
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  float4 x = float4(float(t));
  return x;
}
