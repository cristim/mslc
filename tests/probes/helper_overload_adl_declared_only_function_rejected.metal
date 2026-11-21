// EXPECT: error is declared but never defined
// argument-dependent lookup finds A::f(S), which has no body: calling it in an executable is the
// "declared but never defined" error, not an import (an executable has no Linkage capability).
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { float v; }; float f(S s); }
kernel void k(device float* out [[buffer(0)]]) { A::S s; s.v = 1.0f; out[0] = f(s); }
