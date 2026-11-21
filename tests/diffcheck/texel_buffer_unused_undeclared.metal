// RUN: grid=64 local=16 cases=2
// The same kernel without the declaration: the unread texel buffer is a failure.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read> used [[texture(0)]], texture_buffer<float, access::read> ignored [[texture(1)]],
	device float *out [[buffer(0)]], uint t [[thread_position_in_grid]]) {
	out[t] = used.read(t).x + float(ignored.get_width());
}
