// EXPECT: error in the class template "P" (declared at line 5), instantiated as "P<float>"
// Refs #341: a failure inside an instance is reported at the use, with the template and its arguments.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct P { T v; ~P() {} };
kernel void k(device float* out [[buffer(0)]]) { P<float> p; p.v = 1; out[0] = p.v; }
