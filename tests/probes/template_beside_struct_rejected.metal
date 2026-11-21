// EXPECT: error shares its name with a type
#include <metal_stdlib>
using namespace metal;
struct conv { float v; };
template <typename T> T conv(T c) { return c; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
