// EXPECT: valid
// constexpr int arr[2] = {::n, 4}: ::n is the file-scope n (3), not the runtime local n.
#include <metal_stdlib>
using namespace metal;
constant int n = 3; kernel void kern(device int* out [[buffer(0)]]) { int n = 1; constexpr int arr[2] = {::n, 4}; out[0] = arr[0] + n; }
