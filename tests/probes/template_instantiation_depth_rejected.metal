// EXPECT: error template instantiation depth exceeds 64
// Refs #341: f<0> instantiates f<1>, which instantiates f<2>, without end. Apple stops at 1024.
#include <metal_stdlib>
using namespace metal;
template <int N> float f() { return f<N + 1>(); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f<0>(); }
