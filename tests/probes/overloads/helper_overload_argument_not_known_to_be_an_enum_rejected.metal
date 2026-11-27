// EXPECT: error is not known to be a particular enum
// a++ is an int the emitter cannot tell from an enum, and one overload takes an enum.
#include <metal_stdlib>
using namespace metal;
enum E { ea };
float f(E x) { return 1.0f; }
float f(int x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { int a = 0; out[0] = f(a++); }
