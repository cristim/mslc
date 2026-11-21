// EXPECT: valid
// #232 item f: (void)c; for a "constant float &" parameter of a helper (the parameter is named and unused).
#include <metal_stdlib>
using namespace metal;
constant float K = 2.5f;
float h(constant float &c) { (void)c; return 1.0f; }
float hc(const constant float &, float x) { return x; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = h(K) + hc(K, 2.0f); }
