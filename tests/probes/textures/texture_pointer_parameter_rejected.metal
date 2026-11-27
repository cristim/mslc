// EXPECT: error is a pointer to or an array of
// Apple: "type 'device texture2d<float> *' is not valid for attribute 'buffer'".
#include <metal_stdlib>
using namespace metal;
fragment float4 f(device texture2d<float>* t [[buffer(0)]]) {
  return float4(0.0);
}
