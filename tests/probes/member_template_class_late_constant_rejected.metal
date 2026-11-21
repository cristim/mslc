// EXPECT: error is declared after the class template that uses it
// Refs #341: a member template reads a name from where its class template is, so K below it is refused (Apple rejects K there).
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; template <typename U> U f(U x) const { return x + U(K); } };
constant int K = 2;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ Box<float> b{1.0f}; out[i] = b.f(1.0f); }
