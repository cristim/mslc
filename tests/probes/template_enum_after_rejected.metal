// EXPECT: error beside a function template of the same name
#include <metal_stdlib>
using namespace metal;
template <typename T> T conv(T c) { return c * 2; }
enum conv { A, B };
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
