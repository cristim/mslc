// EXPECT: error no matching function for call to "g"
// Refs #341: g(x, 1) inside an instance names g(SA), which takes one argument (Apple: no matching function);
// an instance's calls are resolved, not trusted, so this is refused instead of emitting a call with a wrong
// argument count (invalid SPIR-V).
#include <metal_stdlib>
using namespace metal;
namespace A { struct SA { float v; }; float g(SA x) { return 9.0f; } template <typename T> float f(T x) { return g(x, 1); } }
kernel void k(device float* out [[buffer(0)]]) { A::SA s; s.v = 1.0f; out[0] = A::f(s); }
