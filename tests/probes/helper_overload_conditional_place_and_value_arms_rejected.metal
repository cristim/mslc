// EXPECT: error a conditional or comma expression as an argument chooses among the overloads
// Apple binds `c ? la : 2.0f` to f(thread float &) (the stored 9 is la), so one place arm is enough to refuse.
#include <metal_stdlib>
using namespace metal;
float f(thread float &x) { x = 9.0f; return 111.0f; }
float f(int x) { return 222.0f; }
kernel void k(device float *o [[buffer(0)]]) { bool c = o[1] > 0.0f; float la = 1.0f; o[0] = f(c ? la : 2.0f); }
