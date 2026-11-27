// EXPECT: error is not a member
// Refs #341: an unknown field in a constructor template's initializer list is diagnosed when instantiated.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> S(U x) : missing(float(x)) {} };
kernel void k(device float* out [[buffer(0)]])
{ S s(2); out[0] = s.a; }
