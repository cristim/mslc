// EXPECT: error argument 1 of the call to "f" is not a form that names a buffer or one of its elements; a whole buffer, &b[i] or p + n is what a pointer parameter takes
// A whole element value is not a pointer: a pointer argument names the buffer
// (&b[i], b + n) and the helper indexes it, so the element is passed by
// writing f(&in[i]) and reading p[0].
#include <metal_stdlib>
using namespace metal;
float f(device float* p, uint i) { return p[i]; }
kernel void helper_device_pointer_middle_rejected(device float* out [[buffer(0)]],
                                                  device float* in [[buffer(1)]],
                                                  uint i [[thread_position_in_grid]])
{ out[i] = f(in[i], 0); }
