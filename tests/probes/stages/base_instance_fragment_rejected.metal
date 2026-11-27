// EXPECT: error builtin "base_instance" is not available in a fragment function
// Apple's compiler reports "invalid 'base_instance' attribute for input declaration in a
// fragment function" (metal 32023.883, macOS 26), and the same for base_vertex in a kernel.
// Vulkan allows BaseInstance in the vertex stage only.
#include <metal_stdlib>
using namespace metal;
fragment float4 base_instance_fragment_rejected(uint bi [[base_instance]]) { return float4(float(bi)); }
