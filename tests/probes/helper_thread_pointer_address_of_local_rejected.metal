// EXPECT: error argument 1 of the call to "f" has to name a local array
//
// Apple accepts f(&x). The parameter is lowered as a pointer to the caller's array, and a
// scalar is not one, so the form is refused rather than miscompiled.
#include <metal_stdlib>
using namespace metal;
void f(thread float *p) { *p = 3.0; }
kernel void helper_thread_pointer_address_of_local_rejected(device float *out [[buffer(0)]])
{ float x = 1.0; f(&x); out[0] = x; }
