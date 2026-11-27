// EXPECT: valid
// uchar b[1] = {::n}: the file-scope n (3) fits, though the constexpr local n (300) would not.
#include <metal_stdlib>
using namespace metal;
constant int n = 3; kernel void kern(device int* out [[buffer(0)]]) { constexpr int n = 300; uchar b[1] = {::n}; out[0] = b[0]; }
