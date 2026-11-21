// EXPECT: error is called before it is declared
// Refs #341: Apple finds h(S) below the template by argument-dependent lookup; mslc refuses it by the order rule.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
template <typename T> struct Box { T v; float f() const { return h(v); } };
float h(S s) { return s.a; }
kernel void k(device float* out [[buffer(0)]]) { Box<S> x; x.v.a = 1.5f; out[0] = x.f(); }
