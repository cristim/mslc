// EXPECT: error most vexing parse
// A bracket inside the array bound. Apple: "array size is not a constant expression".
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a[2] = {1.0, 2.0};
  int b[2] = {0, 1};
  float3 v(float(a[b[1]]));
  o[0] = v.x;
}
