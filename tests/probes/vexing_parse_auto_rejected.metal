// EXPECT: error most vexing parse
// Apple: "function 'v' with deduced return type cannot be used before it is defined".
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  auto v(float(a));
  o[0] = v;
}
