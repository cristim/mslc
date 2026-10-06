// EXPECT: error "::k" names the file-scope declaration, which the local of that name hides here
// Apple accepts this; mslc rejects it rather than lower it.
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0;
kernel void kern(device float* out [[buffer(0)]]) { float k = 2.0; out[0] = ::k + k; }
