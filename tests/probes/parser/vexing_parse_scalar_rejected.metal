// EXPECT: error most vexing parse
// A scalar declared type.
// Apple and C++ read the declaration as a function declaration and reject the use of n as a value.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  int b = 2;
  int n(int(b));
  o[0] = n + 1;
}
