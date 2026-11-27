// EXPECT: error not allowed within a kernel
//
// Apple rejects discard_fragment() in a kernel, with this note:
//   note: function 'air.discard_fragment' is not allowed within a kernel function
// The same reasoning as the vertex case: OpKill terminates an invocation, and a
// compute invocation has no fragment.
#include <metal_stdlib>
using namespace metal;
kernel void discard_fragment_in_a_kernel_is_an_error(device uint *out [[buffer(0)]])
{
    discard_fragment();
    out[0] = 1u;
}