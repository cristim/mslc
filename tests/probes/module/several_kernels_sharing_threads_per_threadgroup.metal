// EXPECT: valid
// A kernel that names its own [[threads_per_threadgroup]] reads the module's
// one size, so a second kernel with the same attribute shares it rather than
// needing a second set of spec constants. This is the shape a shader that
// wants a non-default workgroup size has, with more than one kernel.
#include <metal_stdlib>
using namespace metal;

kernel void a(device int *out [[buffer(0)]],
              uint i [[thread_position_in_grid]],
              uint threads [[threads_per_threadgroup]])
{
  out[i] = (int)threads;
}

kernel void b(device int *out [[buffer(0)]],
              uint i [[thread_position_in_grid]],
              uint threads [[threads_per_threadgroup]])
{
  out[i] = (int)threads + 1;
}