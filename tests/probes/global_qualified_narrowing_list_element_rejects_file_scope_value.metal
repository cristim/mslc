// EXPECT: error constant expression evaluates to 300 which cannot be narrowed
// {::n} with the file-scope n = 300 narrows to uchar, though the constexpr local n (3) would fit. Apple rejects it.
#include <metal_stdlib>
using namespace metal;
constant int n = 300; kernel void kern(device int* out [[buffer(0)]]) { constexpr int n = 3; uchar b[1] = {::n}; out[0] = b[0]; }
