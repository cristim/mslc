// EXPECT: error call to "f" is ambiguous
// Apple accepts the two declarations (a const reference and a by-value parameter are
// different signatures) and reports the call as ambiguous: both are an identity conversion.
#include <metal_stdlib>
using namespace metal;
float f(const thread int &x) { return float(x); }
float f(int x) { return float(x); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1); }
