// EXPECT: error is not a template, and the call gives it explicit template arguments
// Refs #384: a non-template member given "<int>" was accepted with the list dropped when another struct declares a
// member template of that name; Apple rejects it ("'f' does not name a template but is followed by template arguments").
#include <metal_stdlib>
using namespace metal;
struct A { float a; float f(float x) const { return a + x; } };
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
kernel void k(device float* o [[buffer(0)]]) { A x{1.0f}; B y{2.0f}; o[0] = x.f<int>(2) + y.f<int>(3); }
