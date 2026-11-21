// EXPECT: error component of a constant vector chooses among overloads
// Apple finds f(const constant float &) viable for cv[0].x and then errors; dropping the candidate would
// silently call f(int).
#include <metal_stdlib>
using namespace metal;
float f(const constant float &x) { return 111.0f; }
float f(int x) { return 222.0f; }
kernel void k(device float *o [[buffer(0)]], constant float4 *cv [[buffer(1)]]) { o[0] = f(cv[0].x); }
