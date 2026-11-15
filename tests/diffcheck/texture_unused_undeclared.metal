// A texture the kernel ignores and the file does not declare: the run must fail, because the
// texture binding would otherwise be untested while the outputs match.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::read> used [[texture(0)]], texture2d<float, access::read> ignored [[texture(1)]],
	device float *out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[i] = used.read(uint2(i % 4, i / 4 % 4)).x;
}
