// EXPECT: error builtin "base_vertex" is not available in a kernel function
// Apple's compiler reports "invalid 'base_vertex' attribute for input declaration in a kernel
// function" (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
kernel void base_vertex_kernel_rejected(device uint* out [[buffer(0)]], uint bv [[base_vertex]]) { out[0] = bv; }
