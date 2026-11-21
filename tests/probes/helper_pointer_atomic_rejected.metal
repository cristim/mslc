// EXPECT: error parameter "p" of helper function "f" is a pointer outside the thread, device and constant address spaces or to an atomic or a resource
#include <metal_stdlib>
using namespace metal;
float f(device atomic_uint* p) { return atomic_load_explicit(p + 0, memory_order_relaxed); }
kernel void helper_pointer_atomic_rejected(device atomic_uint* out [[buffer(0)]],
                                           uint i [[thread_position_in_grid]])
{ out[i] = f(out); }
