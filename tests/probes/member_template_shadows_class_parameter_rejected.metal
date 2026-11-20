// EXPECT: error shadows a parameter of its class template
// Refs #341: Apple reports "declaration of 'T' shadows template parameter".
#include <metal_stdlib>
using namespace metal;
template <typename T> struct B { T v; template <typename T> T f(T x) const { return x; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ B<float> b{1.0f}; out[i] = b.f(1.0f); }
