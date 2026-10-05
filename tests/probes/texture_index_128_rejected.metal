// EXPECT: error attribute "texture" index 128 is out of bounds
// Apple: "'texture' attribute parameter is out of bounds: must be between 0 and 127".
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(128)]]) {
  return float4(0.0);
}
