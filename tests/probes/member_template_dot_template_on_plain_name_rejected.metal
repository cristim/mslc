// EXPECT: error follows the 'template' keyword
// Refs #384: Apple rejects "x.template f<int>(2)" when f is not a template ("'f' following the 'template' keyword does
// not refer to a template").
#include <metal_stdlib>
using namespace metal;
struct A { float a; float f(float x) const { return a + x; } };
kernel void k(device float* o [[buffer(0)]]) { A x{1.0f}; o[0] = x.template f<int>(2); }
