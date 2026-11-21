// EXPECT: error most vexing parse
// A namespace-qualified declared type.
// Apple and C++ read the declaration as a function declaration and reject the use of t as a value.
#include <metal_stdlib>
using namespace metal;
namespace N { struct T { float a; }; }
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  N::T t(float(a));
  o[0] = t.a;
}
