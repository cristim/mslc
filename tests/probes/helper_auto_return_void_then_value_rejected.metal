// EXPECT: error returns void, so its return cannot carry a value
#include <metal_stdlib>
auto f(int x) { if (x) { return; } return 1; }
kernel void k(device float* o [[buffer(0)]]) { o[0] = float(f(int(o[1]))); }
