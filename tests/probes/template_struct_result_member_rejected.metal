// EXPECT: error swizzles a value that is not a vector
// Refs #341: the type of f(s) is known only once T is deduced, so a member of it is read as a swizzle
// and refused; assign the result to a variable first.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
template <typename T> T f(T x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { S s; s.a = 1.0f; out[0] = f(s).a; }
