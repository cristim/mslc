// EXPECT: error most vexing parse
// A parameter type spelled with a leading "::".
// Apple and C++ read the declaration as a function declaration and reject the use of s as a value.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
kernel void k(device float* o [[buffer(0)]]) {
  S t = {1.0};
  S s(::S(t));
  o[0] = s.a;
}
