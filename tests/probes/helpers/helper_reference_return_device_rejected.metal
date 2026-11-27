// EXPECT: error returns a reference to device memory or to a pointer, which is not lowered yet
// Apple's compiler accepts a device reference return (metal 32023.883, macOS 26). A SPIR-V function here cannot
// return a pointer, and reading a device reference as a value would hide a later write through it.
#include <metal_stdlib>
using namespace metal;
device int& second(device int* p) { return p[1]; }
kernel void helper_reference_return_device_rejected(device int* out [[buffer(0)]]) { out[0] = second(out); }
