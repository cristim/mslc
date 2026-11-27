// EXPECT: valid
// DISASM: %float_1
// DISASM-NOT: %float_2
// Refs #341: a bool template parameter is a bool value in the body, so g(B) calls g(bool) (Apple: 1).
#include <metal_stdlib>
using namespace metal;
float g(bool b) { return 1.0f; }
float g(int i) { return 2.0f; }
template <bool B> float f() { return g(B); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f<true>(); }
