// EXPECT: valid
//
// Inside a namespace its own names, and the file's, are found unqualified; the
// namespace's own name hides the file's of the same spelling.
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0;
constant float other = 10.0;
namespace N {
  constant float k = 2.0;
  struct S { float a; };
  float twice(float x) { return x * k; }
  float use(float x) { S s; s.a = twice(x) + other; return s.a; }
}
kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::use(1.0) + k; }
