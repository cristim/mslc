// EXPECT: error and its returns deduce int and float
#include <metal_stdlib>
auto f(int x) { if (x > 0) { return 1; } return 2.0f; }
kernel void k(device float* o [[buffer(0)]]) { o[0] = float(f(int(o[1]))); }
