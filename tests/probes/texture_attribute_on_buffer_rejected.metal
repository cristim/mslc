// EXPECT: error its attribute is not valid for it
// Apple: "type 'device float *' is not valid for attribute 'texture'".
#include <metal_stdlib>
using namespace metal;
fragment float4 f(device float* p [[texture(0)]]) {
  return float4(0.0);
}
