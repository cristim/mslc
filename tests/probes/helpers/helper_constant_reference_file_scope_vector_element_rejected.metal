// EXPECT: error is a component of a vector
// A file-scope constant vector's element has no address either.
#include <metal_stdlib>
using namespace metal;
constant float4 gv = {1.0f, 2.0f, 3.0f, 4.0f};
float f(const constant float &x) { return x; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = f(gv[1]); }
