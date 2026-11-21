// EXPECT: error no matching function for call to "g"
// Refs #341: slice 5e: Pair<T, int> given Pair<float, uint>; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename A, typename B> struct Pair { A a; B b; };
template <typename T> T g(Pair<T, int> p) { return p.a; }
kernel void k(device float* out [[buffer(0)]]) { Pair<float, uint> p; p.a = 1.5f; p.b = 7; out[0] = g(p); }
