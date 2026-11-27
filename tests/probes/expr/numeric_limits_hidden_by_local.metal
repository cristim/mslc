// EXPECT: valid
// A local named numeric_limits hides the library's, so "numeric_limits < n" is a comparison.
#include <metal_stdlib>
using namespace metal;
kernel void numeric_limits_hidden_by_local(device int* out [[buffer(0)]]) { int numeric_limits = 3; int n = 4; out[0] = numeric_limits < n; }
