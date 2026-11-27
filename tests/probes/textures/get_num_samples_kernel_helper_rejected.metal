// EXPECT: error get_num_samples() is not allowed within a kernel function
// A helper is checked against the stage of the function that calls it, as Apple's compiler does.
#include <metal_stdlib>
using namespace metal;
uint samples() { return get_num_samples(); }
kernel void get_num_samples_kernel_helper_rejected(device uint* out [[buffer(0)]]) { out[0] = samples(); }
