// EXPECT: valid
// Refs #341: a using declaration of a name the template does not spell cannot change what it reads.
#include <metal_stdlib>
using namespace metal;
namespace X { constant int Z = 2; }
template <typename T> struct P { T v; float f() const { return v + 1.0f; } };
using X::Z;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { P<float> p; p.v = 0.5f; out[i] = p.f() + float(Z); }
