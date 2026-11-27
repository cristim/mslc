// EXPECT: error call to "g" passes 1 arguments, and it takes 0
// Refs #341: a template of the helper's name declared below the caller is not a candidate, so the
// helper's own arity check still applies (it used to be skipped: out-of-bounds read, then a crash).
#include <metal_stdlib>
using namespace metal;
float g() { return 3.0f; }
float h(float y) { return g(y); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1.0f); }
template <typename T> T g(T a, T b, T c) { return a; }
