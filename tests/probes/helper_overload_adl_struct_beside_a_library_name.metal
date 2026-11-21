// EXPECT: valid
// DISASM: %float_5_5
// A struct argument matches no library overload of length, so argument-dependent lookup finds
// A::length(S) with nothing to compete with (Apple: 5.5).
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { float v; }; float length(S s) { return 5.5f; } }
kernel void k(device float* out [[buffer(0)]]) { A::S s; s.v = 1.0f; out[0] = length(s); }
