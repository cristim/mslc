// EXPECT: error starts with auto and is not a plain auto
#include <metal_stdlib>
auto& f(thread int& x) { return x; }
kernel void helper_auto_return_reference_rejected_k(device float* o [[buffer(0)]]) { o[0] = float(f(int(o[1]))); }
