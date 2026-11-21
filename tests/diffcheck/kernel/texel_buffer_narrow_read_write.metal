// RUN: grid=64 local=16 cases=3
// DIFFCHECK-TEXEL: a 1
// DIFFCHECK-TEXEL: b 2
// DIFFCHECK-TEXEL: u 1
// DIFFCHECK-TEXEL: h 2
// DIFFCHECK-TEXEL: r 2
// read_write texture_buffers with one and two components (float R, float RG, ushort R, half RG) read and written at
// the thread's own texel, plus a read-only RG one read at the mirrored texel. The update adds to every component of
// the four-component value, so a component the format does not hold must not leak into the next read or write.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read_write> a [[texture(0)]], texture_buffer<float, access::read_write> b [[texture(1)]],
	texture_buffer<ushort, access::read_write> u [[texture(2)]], texture_buffer<half, access::read_write> h [[texture(3)]],
	texture_buffer<float, access::read> r [[texture(4)]], device uint *width [[buffer(0)]], uint t [[thread_position_in_grid]]) {
	uint n = a.get_width();
	a.write(a.read(t) * 2.0f + r.read(n - 1 - t), t);
	b.write(b.read(t) + float4(1.0f, 2.0f, 3.0f, 4.0f), t);
	u.write(u.read(t) + ushort4(1, 2, 3, 4), t);
	h.write(h.read(t) * 0.5h + half4(0.25h), t);
	width[t] = n;
}
