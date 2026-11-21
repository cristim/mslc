// RUN: grid=64 local=16 cases=3
// read_write texture_buffers (float, ushort, half) read and written at the thread's own texel, plus
// a read-only one read at the mirrored texel. Two float read_write buffers take different updates, so
// swapping their bindings, reading at the wrong texel or writing at another all change a buffer.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read_write> a [[texture(0)]], texture_buffer<float, access::read_write> b [[texture(1)]],
	texture_buffer<ushort, access::read_write> u [[texture(2)]], texture_buffer<half, access::read_write> h [[texture(3)]],
	texture_buffer<float, access::read> r [[texture(4)]], device uint *width [[buffer(0)]], uint t [[thread_position_in_grid]]) {
	uint n = a.get_width();
	a.write(a.read(t) * 2.0f + r.read(n - 1 - t), t);
	b.write(b.read(t) + float4(1.0f, 2.0f, 3.0f, 4.0f), t);
	u.write(u.read(t) + ushort4(1, 2, 3, 4), t);
	h.write(h.read(t) * 0.5h, t);
	width[t] = n;
}
