// EXPECT: error whose result type is known only once it is deduced
// Refs #341: id(s) has the type of s, and A::g is found through it by argument-dependent lookup; the
// emitter learns that type only when it evaluates the call, so a call whose lookup depends on it is
// refused (Apple calls A::g(S)).
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { float v; }; float g(S s) { return s.v; } }
template <typename T> T id(T x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { A::S s; s.v = 1.0f; out[0] = g(id(s)); }
