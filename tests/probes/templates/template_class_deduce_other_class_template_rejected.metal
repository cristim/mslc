// EXPECT: error no matching function for call to "z"; candidates: ; templates: template
// Refs #341: slice 5e: an instance of another class template does not match Tag<T>; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Tag { float v; };
template <typename T> struct Tag2 { float v; };
template <typename T> T z(Tag<T> q) { return T(q.v); }
kernel void k(device float* out [[buffer(0)]]) { Tag2<int> q; q.v = 2.5f; out[0] = float(z(q)); }
