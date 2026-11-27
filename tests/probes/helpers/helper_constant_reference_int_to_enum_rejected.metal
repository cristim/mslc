// EXPECT: error does not convert to it
#include <metal_stdlib>
using namespace metal;
enum E { X, Y };
constant int KI = 1;
int f(const constant E &x) { return 1; }
kernel void k(device int *o [[buffer(0)]]) { o[0] = f(KI); }
