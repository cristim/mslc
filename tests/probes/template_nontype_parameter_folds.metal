// EXPECT: valid
// DISASM: %int_3
// DISASM: %int_4
// Refs #341: a non-type parameter is a constant in the instance: f<3> and f<4> are two functions,
// one returning 3 and one 4.
#include <metal_stdlib>
using namespace metal;
template <int N> float f() { return float(N); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f<3>() + f<4>(); }
