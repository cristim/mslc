// EXPECT: error argument types are declared after the template
// Refs #341: slice 5e: Tag<P> with P declared after the function template is refused by the 5d rule (Apple accepts it).
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Tag { float v; };
template <typename T> float f(Tag<T> w) { return w.v; }
struct P { float x; };
kernel void k(device float* out [[buffer(0)]]) { Tag<P> b; b.v = 4.0f; out[0] = f(b); }
