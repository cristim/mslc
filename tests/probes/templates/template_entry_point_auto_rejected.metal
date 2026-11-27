// EXPECT: error is an entry point declared auto
// Apple refuses it: "invalid return type 'auto' for kernel function" (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
using namespace metal;
template <typename T> kernel auto k(device T* out [[buffer(0)]]) { out[0] = 1; }
template [[host_name("k_f")]] kernel void k<float>(device float*);
