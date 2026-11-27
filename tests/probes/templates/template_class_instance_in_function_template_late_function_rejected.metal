// EXPECT: error "tw" is called before it is declared
// Refs #341: slice 5d: Apple rejects W<int> (tw is neither visible in the template nor found by lookup for an int); so does mslc.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct W { T v; T get() const { return T(tw(v)); } };
template <typename T> float h(T x) { W<T> w; w.v = x; return float(w.get()); }
int tw(int a) { return a + 1; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1); }
