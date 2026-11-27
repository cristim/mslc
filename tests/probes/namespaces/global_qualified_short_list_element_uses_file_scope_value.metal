// EXPECT: valid
// short b[1] = {::n}: the file-scope short n fits; the constexpr local int n (70000) would not.
#include <metal_stdlib>
using namespace metal;
constant short n = 3; kernel void kern(device int* out [[buffer(0)]]) { constexpr int n = 70000; short b[1] = {::n}; out[0] = b[0]; }
