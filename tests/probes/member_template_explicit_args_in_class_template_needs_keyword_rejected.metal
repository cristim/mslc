// EXPECT: error write ".template f<...>(...)"
// Refs #384: in a class template whose receiver type is a parameter, Apple requires "in.template f<float>(x)" and
// rejects "in.f<float>(x)" ("use 'template' keyword to treat 'f' as a dependent template name").
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> struct W { float g(T in) const { return in.f<float>(1.0f); } };
kernel void k(device float* o [[buffer(0)]]) { B y{2.0f}; W<B> w; o[0] = w.g(y); }
