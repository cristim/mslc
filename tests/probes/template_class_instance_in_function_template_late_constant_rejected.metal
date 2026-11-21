// EXPECT: error declared after the class template
// Refs #341: slice 5d: Apple rejects W<int> (K is undeclared in W); the instance is made above K, so the late name is found at the end of the file.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct W { T v; T get() const { return v + K; } };
constant int K = 5;
template <typename T> float h(T x) { W<T> w; w.v = x; return float(w.get()); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1); }
