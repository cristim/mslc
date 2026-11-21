// EXPECT: error ambiguous
// A constant lvalue is an exact match for a by-value parameter and for a constant reference: Apple
// reports the call as ambiguous, and so does mslc.
#include <metal_stdlib>
using namespace metal;
constant float K = 2.5f;
float pick(float c) { return 111.0f; }
float pick(const constant float &c) { return 222.0f; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = pick(K); }
