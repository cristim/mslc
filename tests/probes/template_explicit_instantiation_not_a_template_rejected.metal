// EXPECT: error does not name a function template
// Apple rejects this too (it reads the declaration as a variable definition).
#include <metal_stdlib>
using namespace metal;
float g(float x) { return x; }
template float g<float>(float);
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
