// EXPECT: valid
// Refs #341: v.get() is a member call: a later struct with a get() member does not make it a lookup of free functions.
#include <metal_stdlib>
using namespace metal;
struct S { float a; float get() const { return a; } };
template <typename T> struct Box { T v; float get() const { return v.get(); } };
struct R { float b; float get() const { return b * 2.0f; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { Box<S> x; x.v.a = 1.5f; R r; r.b = 1; out[i] = x.get() + r.get(); }
