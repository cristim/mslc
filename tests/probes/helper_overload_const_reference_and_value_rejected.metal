// EXPECT: error overloading "f" is not lowered
// Apple accepts the overload: a const reference and a by-value parameter are
// different signatures. Master's sameParameterTypes treated them as the same
// parameter (both not a mutable reference) and reported "redefinition of f".
#include <metal_stdlib>
using namespace metal;
float f(const thread int &x) { return float(x); }
float f(int x) { return float(x); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1); }
