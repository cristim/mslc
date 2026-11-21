// EXPECT: error after a for initialiser
// Apple accepts the unused declaration and rejects the use; mslc's for initialiser takes no constructor-style
// parentheses at all, so this is refused either way.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  for (float3 v(float(a)); v.x < 2.0; ) { o[0] = 1.0; break; }
}
