// EXPECT: error most vexing parse
// auto in a for initialiser. Apple: "function 'v' with deduced return type cannot be used before it is defined".
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  for (auto v(float(a)); v < 2.0; ) { o[0] = 1.0; break; }
}
