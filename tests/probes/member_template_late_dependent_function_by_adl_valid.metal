// EXPECT: valid
// Refs #341: a dependent call in a member template also finds, by argument-dependent lookup where the template is
// used, what is declared below the struct: g(A) is 2.0, and a scalar argument still binds the g(float) above it.
#include <metal_stdlib>
using namespace metal;
struct A { float v; };
float g(float x) { return 1.0f; }
struct S { template <typename U> float f(U x) const { return g(x); } };
float g(A x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s; A a{1.0f}; out[i] = s.f(a) * 10.0f + s.f(3.0f); }
