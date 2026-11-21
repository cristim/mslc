// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// A device or constant element can never bind a thread reference, so that overload drops out
// and f(buf[0]) is f(float) (Apple), whatever the reference's type.
#include <metal_stdlib>
using namespace metal;
float f(float x) { return 1.5f; }
float f(thread half &x) { return 7.5f; }
kernel void k(device float* buf [[buffer(0)]], constant float* cbuf [[buffer(1)]])
{ buf[1] = f(buf[0]) + f(cbuf[0]); }
