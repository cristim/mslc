// EXPECT: error a using declaration or directive is made after the class template
// Refs #341: Apple rejects K here (X::K is not visible from the definition).
#include <metal_stdlib>
using namespace metal;
namespace X { constant int K = 2; }
template <typename T> struct P { T v; float f() const { return float(K) + float(v); } };
using namespace X;
kernel void k(device float* out [[buffer(0)]]) { P<float> p; p.v = 0.5f; out[0] = p.f(); }
