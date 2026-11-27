// EXPECT: valid
// A local named access does not hide the library's access:: before "::".
#include <metal_stdlib>
using namespace metal;
kernel void kern(device float* out [[buffer(0)]]) { float access = 1; using access::read; out[0] = access; }
