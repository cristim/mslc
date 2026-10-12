// EXPECT: error discard_fragment() is not allowed within a kernel function
#include <metal_stdlib>
using namespace metal;
float maybe_drop(float x) { if (x > 0.5f) discard_fragment(); return x; }
kernel void discard_helper_called_from_kernel_rejected(device float *o [[buffer(0)]]) { o[0] = maybe_drop(0.75f); }
