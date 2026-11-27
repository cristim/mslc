// EXPECT: error argument 1 of the call to "f" has to name a local array
//
// Apple accepts f(a + 1); an offset pointer cannot be an array of known length here.
#include <metal_stdlib>
using namespace metal;
void f(thread float *p) { p[0] = 3.0; }
kernel void helper_thread_pointer_arithmetic_argument_rejected(device float *out [[buffer(0)]])
{ float a[4]; a[1] = 0.0; f(a + 1); out[0] = a[1]; }
