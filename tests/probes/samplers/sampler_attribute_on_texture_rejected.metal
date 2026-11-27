// EXPECT: error its attribute is not valid for it
// Apple: "type 'texture2d<float>' is not valid for attribute 'sampler'".
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[sampler(0)]]) {
  return float4(0.0);
}
