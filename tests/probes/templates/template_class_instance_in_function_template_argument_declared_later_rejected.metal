// EXPECT: error its argument types are declared after the template
// Refs #341: slice 5d: Box<P> is named in f, but P is declared after f, so the name state f is read in cannot see Box<P>.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; T get() const { return v; } };
template <typename T> float f(T x) { Box<T> b; b.v = x; return b.get().a; }
struct P { float a; };
kernel void k(device float* out [[buffer(0)]]) { P p; p.a = 6.0f; out[0] = f(p); }
