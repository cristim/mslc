// EXPECT: valid
// DISASM: OpFunctionCall %float
// Refs #341: a prototype and its definition are one template; a call between them is a call to it,
// and the body sees what is declared above the definition (g is declared after the prototype).
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x);
float g(float x) { return x; }
template <typename T> float f(T x) { return g(x); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1); }
