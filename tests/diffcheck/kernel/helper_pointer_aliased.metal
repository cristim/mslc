// RUN: grid=64 local=8 seed=1 cases=3
#include <metal_stdlib>
using namespace metal;
// The same buffer passed as two pointer parameters: the store through the first must be seen by
// the load through the second, as Metal's pointers may alias.
static float bump(device float* a, device float* b, uint i) { a[i] = a[i] * 2.0f + 1.0f; return b[i] + 1.0f; }
kernel void helper_pointer_aliased(device float* out [[buffer(0)]], uint g [[thread_position_in_grid]])
{
	out[g] = bump(out, out, g);
}
