// EXPECT: valid
// Refs #341: length is the library's; a later Q::length(int) in another namespace is not what the template calls.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
namespace Q { float length(int x) { return 0.0f; } }
template <typename T> struct Box { T v; float f() const { return length(float2(v.a, 0)); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { Box<S> x; x.v.a = 1.5f; out[i] = x.f(); }
