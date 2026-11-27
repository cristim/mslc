// EXPECT: error write ".template f<...>(...)"
// Refs #384: the same rule for a function template; Apple rejects "in.f<float>(x)" when in has a dependent type.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> float g(T in) { return in.f<float>(1.0f); }
kernel void k(device float* o [[buffer(0)]]) { B y{2.0f}; o[0] = g(y); }
