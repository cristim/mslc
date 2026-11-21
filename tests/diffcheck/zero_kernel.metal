#include <metal_stdlib>
using namespace metal;
// A kernel whose output is all zero on both backends: the harness must refuse to call
// that a pass unless the file declares it.
kernel void k(device uint* o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	o[i] = 0u;
}
