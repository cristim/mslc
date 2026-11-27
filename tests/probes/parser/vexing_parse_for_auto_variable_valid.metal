// EXPECT: valid
// A plain variable in parentheses after auto in a for initialiser is a variable.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  for (auto v(a); v < 2.0; ) { o[0] = 1.0; break; }
}
