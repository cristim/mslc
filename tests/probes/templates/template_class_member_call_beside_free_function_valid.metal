// EXPECT: valid
// Refs #341: v.get() is a member call even when free functions named get exist above and below the template.
#include <metal_stdlib>
using namespace metal;
struct S { float a; float get() const { return a; } };
float get(S s) { return 7.0f; }
template <typename T> struct Box { T v; float f() const { return v.get(); } };
float get(S s, int x) { return 9.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { Box<S> x; x.v.a = 1.5f; out[i] = x.f(); }
