// EXPECT: error no matching function for call to "f"
// Refs #341: 300 does not fit an uchar, so the explicit argument does not fit the template.
#include <metal_stdlib>
using namespace metal;
template <uchar N> float f() { return float(N); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f<300>(); }
