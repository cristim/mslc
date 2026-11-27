// EXPECT: error a braced list deduces no return type
#include <metal_stdlib>
auto f(int x) { return {1, 2}; }
kernel void k(device float* o [[buffer(0)]]) { o[0] = float(f(int(o[1]))); }
