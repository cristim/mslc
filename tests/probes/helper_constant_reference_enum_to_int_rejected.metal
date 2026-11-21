// EXPECT: error is not the type of parameter
#include <metal_stdlib>
using namespace metal;
enum E { X, Y };
constant E KE = Y;
int f(const constant int &x) { return x; }
kernel void k(device int *o [[buffer(0)]]) { o[0] = f(KE); }
