// EXPECT: error follows the 'template' keyword and is not a template
// Refs #386: "x.template p(a)" on a plain member is rejected by Apple ("'p' following the 'template' keyword does
// not refer to a template) even when another struct declares a member template of that name.
#include <metal_stdlib>
using namespace metal;
struct A { float a; float p(float x) const { return a + x; } };
struct B { float b; template <typename U> float p(U x) const { return b + float(x); } };
kernel void k(device float* o [[buffer(0)]]) { A x{1.0f}; B y{2.0f}; o[0] = x.template p(2.0f) + y.p(1.0f); }
