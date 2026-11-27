// EXPECT: error most vexing parse
// A helper function body is parsed the same way as a kernel's.
#include <metal_stdlib>
using namespace metal;
float h(float a) {
  float3 v(float(a));
  return v.x;
}
kernel void k(device float* o [[buffer(0)]]) { o[0] = h(1.0); }
