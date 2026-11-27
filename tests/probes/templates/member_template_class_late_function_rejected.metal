// EXPECT: error is declared after the class template
// Refs #341: the late-declaration rule of the class template covers its member templates too (clang finds the later g(int) by ADL for an enum or struct argument).
#include <metal_stdlib>
using namespace metal;
float g(float x) { return 1.0f; }
template <typename T> struct Box { T v; template <typename U> float f(U x) const { return g(x); } };
float g(int x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ Box<float> b{1.0f}; out[i] = b.f(3.0f); }
