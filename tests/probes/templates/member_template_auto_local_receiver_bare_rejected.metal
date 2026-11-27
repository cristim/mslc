// EXPECT: error write ".template f<...>(...)"
// Refs #386: an auto local takes its type from a dependent initialiser, so the bare call is refused (Apple rejects it).
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> float g(T in) { auto a = in; return a.f<float>(1.0f); }
kernel void k(device float* o [[buffer(0)]]) { B y{2.0f}; o[0] = g(y); }
