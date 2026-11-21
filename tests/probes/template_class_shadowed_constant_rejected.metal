// EXPECT: error is declared after the class template that uses it
// Refs #341: the template's K is ::K (1); N::K declared below it must not be the one an instance reads (Apple: 1).
#include <metal_stdlib>
using namespace metal;
constant int K = 1;
namespace N {
template <typename T> struct P { T v[K]; int n() const { return K; } };
constant int K = 2;
}
kernel void k(device float* out [[buffer(0)]]) { N::P<float> p; p.v[0] = 1; out[0] = float(p.n()); }
